/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

// editor
#if 0
STATIC(SessionLoad, &, 0)
STATIC(SessionClose, &, 0)
#endif
//STATIC(EverySecond, get_timers().second, 0) // XXX
STATIC(ConfigChanged, &ARDOUR::Config->ParameterChanged, 1)

// engine instance
ENGINE(EngineRunning, Running, 0)
ENGINE(EngineStopped, Stopped, 0)
ENGINE(EngineHalted, Halted, 1)
ENGINE(EngineDeviceListChanged, DeviceListChanged, 0)
ENGINE(BufferSizeChanged, BufferSizeChanged, 1)
ENGINE(SampleRateChanged, SampleRateChanged, 1)

// session static
STATIC(FeedbackDetected, &ARDOUR::Session::FeedbackDetected, 0)
STATIC(SuccessfulGraphSort, &ARDOUR::Session::SuccessfulGraphSort, 0)
STATIC(StartTimeChanged, &ARDOUR::Session::StartTimeChanged, 1)
STATIC(EndTimeChanged, &ARDOUR::Session::EndTimeChanged, 1)
STATIC(Exported, &ARDOUR::Session::Exported, 2)

// session specific (re-subscribe when session changes)
SESSION(SessionConfigChanged, config.ParameterChanged, 1)
SESSION(TransportStateChange, TransportStateChange, 0)
SESSION(DirtyChanged, DirtyChanged, 0)
SESSION(StateSaved, StateSaved, 1)
SESSION(Xrun, Xrun, 1)
SESSION(TransportLooped, TransportLooped, 0)
SESSION(SoloActive, SoloActive, 1)
SESSION(SoloChanged, SoloChanged, 0)
SESSION(IsolatedChanged, IsolatedChanged, 0)
SESSION(MonitorChanged, MonitorChanged, 0)
SESSION(RecordStateChanged, RecordStateChanged, 0)
SESSION(RecordArmStateChanged, RecordArmStateChanged, 0)
SESSION(AudioLoopLocationChanged, auto_loop_location_changed, 1)
SESSION(AudioPunchLocationChanged, auto_punch_location_changed, 1)
SESSION(LocationsModified, locations_modified, 0)
SESSION(AuditionActive, AuditionActive, 1)
SESSION(BundleAddedOrRemoved, BundleAddedOrRemoved, 0)
SESSION(PositionChanged, PositionChanged, 1)
SESSION(Located, Located, 0)
SESSION(RoutesReconnected, session_routes_reconnected, 0)
SESSION(RouteAdded, RouteAdded, 1)
SESSION(RouteAddedOrRemoved, RouteAddedOrRemoved, 1)
SESSION(RouteGroupPropertyChanged, RouteGroupPropertyChanged, 1)
SESSION(RouteAddedToRouteGroup, RouteAddedToRouteGroup, 2)
SESSION(RouteRemovedFromRouteGroup, RouteRemovedFromRouteGroup, 2)
SESSION(StepEditStatusChange, StepEditStatusChange, 1)
SESSION(RouteGroupAdded, route_group_added, 1)
SESSION(RouteGroupRemoved, route_group_removed, 0)
SESSION(RouteGroupsReordered, route_groups_reordered, 0)

// route static globals
STATIC(SyncOrderKeys, &Route::SyncOrderKeys, 0)

// plugin manager instance
STATIC(PluginListChanged, &(PluginManager::instance().PluginListChanged), 0)
STATIC(PluginStatusesChanged, &(PluginManager::instance().PluginStatusesChanged), 0)

// Diskstream static global
STATIC(DiskOverrun, &ARDOUR::Diskstream::DiskOverrun, 0)
STATIC(DiskUnderrun, &ARDOUR::Diskstream::DiskUnderrun, 0)

// Region static
STATIC(RegionPropertyChanged, &ARDOUR::Region::RegionPropertyChanged, 2)

// TODO per track/route signals,
// TODO per plugin actions / controllables
// TODO per region actions
//SESSIONOBJECT(PropertyChanged, &ARDOUR::Stateful::PropertyChanged, 1)

// TODO any location action