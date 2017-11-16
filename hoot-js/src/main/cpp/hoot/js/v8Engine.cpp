/*
 * This file is part of Hootenanny.
 *
 * Hootenanny is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * --------------------------------------------------------------------
 *
 * The following copyright notices are generated automatically. If you
 * have a new notice to add, please use the format:
 * " * @copyright Copyright ..."
 * This will properly maintain the copyright information. DigitalGlobe
 * copyrights will be updated automatically.
 *
 * @copyright Copyright (C) 2017 DigitalGlobe (http://www.digitalglobe.com/)
 */
#include "v8Engine.h"

using namespace std;
using namespace v8;

namespace hoot
{

auto_ptr<v8Engine> v8Engine::_theInstance;

v8Engine::v8Engine()
{
  V8::InitializeICUDefaultLocation(NULL);
  V8::InitializeExternalStartupData(NULL);
  //  Setup and initialize the platform
  _platform.reset(platform::CreateDefaultPlatform());
  V8::InitializePlatform(_platform.get());
  //  Initialize v8
  V8::Initialize();
  //  Create the main isolate
  _allocator.reset(ArrayBuffer::Allocator::NewDefaultAllocator());
  Isolate::CreateParams params;
  params.array_buffer_allocator = _allocator.get();
  _isolate = Isolate::New(params);
  _isolate->Enter();
}

v8Engine::~v8Engine()
{
  _isolate->Exit();
  //  Dispose of the v8 subsystem
  _isolate->Dispose();
  V8::Dispose();
  //  Shutdown the platform
  V8::ShutdownPlatform();
}

v8Engine& v8Engine::getInstance()
{
  if (_theInstance.get() == 0)
  {
    _theInstance.reset(new v8Engine());
  }
  return *_theInstance;
}

}