/*
    Copyright (C) 1999-2009 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#ifndef PLATFORM_WINDOWS
#include <poll.h>
#endif

#include "pbd/error.h"
#include "pbd/pthread_utils.h"
#include "ardour/butler.h"
#include "ardour/io.h"
#include "ardour/midi_diskstream.h"
#include "ardour/session.h"
#include "ardour/track.h"
#include "ardour/auditioner.h"

#include "i18n.h"

using namespace PBD;

namespace ARDOUR {

Butler::Butler(Session& s)
	: SessionHandleRef (s)
	, thread()
	, have_thread (false)
	, audio_dstream_capture_buffer_size(0)
	, audio_dstream_playback_buffer_size(0)
	, midi_dstream_buffer_size(0)
	, pool_trash(16)
{
	g_atomic_int_set(&should_do_transport_work, 0);
	SessionEvent::pool->set_trash (&pool_trash);

        Config->ParameterChanged.connect_same_thread (*this, boost::bind (&Butler::config_changed, this, _1));
}

Butler::~Butler()
{
	terminate_thread ();
}

void
Butler::config_changed (std::string p)
{
        if (p == "playback-buffer-seconds") {
                /* size is in Samples, not bytes */
                audio_dstream_playback_buffer_size = (uint32_t) floor (Config->get_audio_playback_buffer_seconds() * _session.frame_rate());
                _session.adjust_playback_buffering ();
        } else if (p == "capture-buffer-seconds") {
                audio_dstream_capture_buffer_size = (uint32_t) floor (Config->get_audio_capture_buffer_seconds() * _session.frame_rate());
                _session.adjust_capture_buffering ();
        }
}

#ifndef PLATFORM_WINDOWS
int
Butler::setup_request_pipe ()
{
	if (pipe (request_pipe)) {
		error << string_compose(_("Cannot create transport request signal pipe (%1)"),
				strerror (errno)) << endmsg;
		return -1;
	}

	if (fcntl (request_pipe[0], F_SETFL, O_NONBLOCK)) {
		error << string_compose(_("UI: cannot set O_NONBLOCK on butler request pipe (%1)"),
				strerror (errno)) << endmsg;
		return -1;
	}

	if (fcntl (request_pipe[1], F_SETFL, O_NONBLOCK)) {
		error << string_compose(_("UI: cannot set O_NONBLOCK on butler request pipe (%1)"),
				strerror (errno)) << endmsg;
		return -1;
	}
	return 0;
}
#endif

int
Butler::start_thread()
{
	const float rate = (float)_session.frame_rate();

	/* size is in Samples, not bytes */
	audio_dstream_capture_buffer_size = (uint32_t) floor (Config->get_audio_capture_buffer_seconds() * rate);
	audio_dstream_playback_buffer_size = (uint32_t) floor (Config->get_audio_playback_buffer_seconds() * rate);

	/* size is in bytes
	 * XXX: Jack needs to tell us the MIDI buffer size
	 * (i.e. how many MIDI bytes we might see in a cycle)
	 */
	midi_dstream_buffer_size = (uint32_t) floor (Config->get_midi_track_buffer_seconds() * rate);

	MidiDiskstream::set_readahead_frames ((framecnt_t) (Config->get_midi_readahead() * rate));

	should_run = false;

#ifndef PLATFORM_WINDOWS
	if (setup_request_pipe() != 0) return -1;
#endif

	if (pthread_create_and_store ("disk butler", &thread, _thread_work, this)) {
		error << _("Session: could not create butler thread") << endmsg;
		return -1;
	}

	//pthread_detach (thread);
	have_thread = true;
	return 0;
}

void
Butler::terminate_thread ()
{
	if (have_thread) {
		void* status;
		queue_request (Request::Quit);
		pthread_join (thread, &status);
	}
}

void *
Butler::_thread_work (void* arg)
{
	SessionEvent::create_per_thread_pool ("butler events", 4096);
	pthread_set_name (X_("butler"));
	return ((Butler *) arg)->thread_work ();
}

bool
Butler::wait_for_requests ()
{
#ifndef PLATFORM_WINDOWS
	struct pollfd pfd[1];

	pfd[0].fd = request_pipe[0];
	pfd[0].events = POLLIN|POLLERR|POLLHUP;

	while(true) {
		if (poll (pfd, 1, -1) < 0) {

			if (errno == EINTR) {
				continue;
			}

			error << string_compose (_("poll on butler request pipe failed (%1)"),
					strerror (errno))
				<< endmsg;
			break;
		}

		if (pfd[0].revents & ~POLLIN) {
			error << string_compose (_("Error on butler thread request pipe: fd=%1 err=%2"), pfd[0].fd, pfd[0].revents) << endmsg;
			break;
		}

		if (pfd[0].revents & POLLIN) {
			return true;
		}
	}
	return false;
#else
	m_request_sem.wait ();
	return true;
#endif
}

bool
Butler::dequeue_request (Request::Type& r)
{
#ifndef PLATFORM_WINDOWS
	char req;
	size_t nread = ::read (request_pipe[0], &req, sizeof (req));
	if (nread == 1) {
		r = (Request::Type) req;
		return true;
	} else if (nread == 0) {
		return false;
	} else if (errno == EAGAIN) {
		return false;
	} else {
		fatal << _("Error reading from butler request pipe") << endmsg;
		abort(); /*NOTREACHED*/
	}
#else
	r = (Request::Type) m_request_state.get();
#endif
	return false;
}

	void *
Butler::thread_work ()
{
	uint32_t err = 0;

	bool disk_work_outstanding = false;
	RouteList::iterator i;

	while (true) {
		if(!disk_work_outstanding) {
			if (wait_for_requests ()) {
				Request::Type req;

				/* empty the pipe of all current requests */
#ifdef PLATFORM_WINDOWS
				dequeue_request (req);
				{
#else
				while(dequeue_request(req)) {
#endif
					switch (req) {

					case Request::Run:
						should_run = true;
						break;

					case Request::Pause:
						should_run = false;
						break;

					case Request::Quit:
						return 0;
						abort(); /*NOTREACHED*/
						break;

					default:
						break;
					}
				}
			}
		}


restart:
		disk_work_outstanding = false;

		if (transport_work_requested()) {
			_session.butler_transport_work ();
		}

		frameoffset_t audition_seek;
		if (should_run && _session.is_auditioning()
				&& (audition_seek = _session.the_auditioner()->seek_frame()) >= 0) {
			boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (_session.the_auditioner());
			tr->seek(audition_seek);
			_session.the_auditioner()->seek_response(audition_seek);
		}

		boost::shared_ptr<RouteList> rl = _session.get_routes();

		RouteList rl_with_auditioner = *rl;
		rl_with_auditioner.push_back (_session.the_auditioner());

//		for (i = dsl->begin(); i != dsl->end(); ++i) {
//			cerr << "BEFORE " << (*i)->name() << ": pb = " << (*i)->playback_buffer_load() << " cp = " << (*i)->capture_buffer_load() << endl;
//		}

		for (i = rl_with_auditioner.begin(); !transport_work_requested() && should_run && i != rl_with_auditioner.end(); ++i) {

			boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);

			if (!tr) {
				continue;
			}

			boost::shared_ptr<IO> io = tr->input ();

			if (io && !io->active()) {
				/* don't read inactive tracks */
				continue;
			}

			switch (tr->do_refill ()) {
			case 0:
				break;
				
			case 1:
				disk_work_outstanding = true;
				break;

			default:
				error << string_compose(_("Butler read ahead failure on dstream %1"), (*i)->name()) << endmsg;
				break;
			}

		}

		if (i != rl_with_auditioner.begin() && i != rl_with_auditioner.end()) {
			/* we didn't get to all the streams */
			disk_work_outstanding = true;
		}

		if (!err && transport_work_requested()) {
			goto restart;
		}

		for (i = rl->begin(); !transport_work_requested() && should_run && i != rl->end(); ++i) {
			// cerr << "write behind for " << (*i)->name () << endl;

			boost::shared_ptr<Track> tr = boost::dynamic_pointer_cast<Track> (*i);

			if (!tr) {
				continue;
			}

			/* note that we still try to flush diskstreams attached to inactive routes
			 */

			switch (tr->do_flush (ButlerContext)) {
			case 0:
				break;
				
			case 1:
				disk_work_outstanding = true;
				break;

			default:
				err++;
				error << string_compose(_("Butler write-behind failure on dstream %1"), (*i)->name()) << endmsg;
				/* don't break - try to flush all streams in case they
				   are split across disks.
				*/
			}
		}

		if (err && _session.actively_recording()) {
			/* stop the transport and try to catch as much possible
			   captured state as we can.
			*/
			_session.request_stop ();
		}

		if (i != rl->begin() && i != rl->end()) {
			/* we didn't get to all the streams */
			disk_work_outstanding = true;
		}

		if (!err && transport_work_requested()) {
			goto restart;
		}

		if (!disk_work_outstanding) {
			_session.refresh_disk_space ();
		}


		{
			Glib::Threads::Mutex::Lock lm (request_lock);

			if (should_run && (disk_work_outstanding || transport_work_requested())) {
//				for (DiskstreamList::iterator i = dsl->begin(); i != dsl->end(); ++i) {
//					cerr << "AFTER " << (*i)->name() << ": pb = " << (*i)->playback_buffer_load() << " cp = " << (*i)->capture_buffer_load() << endl;
//				}

				goto restart;
			}

			paused.signal();
		}

		empty_pool_trash ();
	}

	return (0);
}

void
Butler::schedule_transport_work ()
{
	g_atomic_int_inc (&should_do_transport_work);
	summon ();
}

void
Butler::queue_request (Request::Type r)
{
#ifndef PLATFORM_WINDOWS
	char c = r;
	(void) ::write (request_pipe[1], &c, 1);
#else
	m_request_state.set (r);
	m_request_sem.post ();
#endif
}

void
Butler::summon ()
{
	queue_request (Request::Run);
}

void
Butler::stop ()
{
	Glib::Threads::Mutex::Lock lm (request_lock);
	queue_request (Request::Pause);
	paused.wait(request_lock);
}

void
Butler::wait_until_finished ()
{
	Glib::Threads::Mutex::Lock lm (request_lock);
	queue_request (Request::Pause);
	paused.wait(request_lock);
}

bool
Butler::transport_work_requested () const
{
	return g_atomic_int_get(&should_do_transport_work);
}

void
Butler::empty_pool_trash ()
{
	/* look in the trash, deleting empty pools until we come to one that is not empty */

	RingBuffer<CrossThreadPool*>::rw_vector vec;
	pool_trash.get_read_vector (&vec);

	guint deleted = 0;

	for (int i = 0; i < 2; ++i) {
		for (guint j = 0; j < vec.len[i]; ++j) {
			if (vec.buf[i][j]->empty()) {
				delete vec.buf[i][j];
				++deleted;
			} else {
				/* found a non-empty pool, so stop deleting */
				if (deleted) {
					pool_trash.increment_read_idx (deleted);
				}
				return;
			}
		}
	}

	if (deleted) {
		pool_trash.increment_read_idx (deleted);
	}
}

void
Butler::drop_references ()
{
	cerr << "Butler drops pool trash\n";
	SessionEvent::pool->set_trash (0);
}


} // namespace ARDOUR

