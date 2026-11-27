# macOS: the client crashes drawing far tiles' packed vertices on the GL 2.1 driver

Found by the CI owner, 2026-11-27, in release.yml's macOS "Shell" step (branch run 46069031677,
which carried b165b8c8 before it reached origin/master; master's own macOS run 46055042140, without
it, didn't crash).

**What happens.** On macOS the OpenGL 3 driver asks SDL for a 3.2 *compatibility* context
(`library/vendors/irr/src/CIrrDeviceSDL.cpp`, `EDT_OPENGL3`). macOS has no such thing (only 2.1
legacy or 3.2+ core), so it fails five times and the engine falls back to the legacy `COpenGLDriver`
at GL 2.1. b165b8c8 ("Memory sprint") gave far tiles `EVT_PACKED` vertices, wired only in the GL3
driver (`OpenGL/Driver.cpp`'s `vtPacked`). The legacy driver reads them as if they were another
type and runs off the end of the buffer:

```
* thread #1, name = 'CrBrowserMain', stop reason = EXC_BAD_ACCESS (code=2, address=0x11c0be3c004)
  frame #1: GLEngine`glDrawElements_IMM_Exec + 716
  frame #2: terraverse`video::COpenGLDriver::drawVertexPrimitiveList(...) + 1124
  frame #3: terraverse`video::COpenGLDriver::drawBuffers(...) + 356
  frame #4: terraverse`... planetarium::Planetarium::draw(...)::$_0 ...
  frame #5: terraverse`planetarium::SystemLayer::draw(...)
  frame #6: terraverse`planetarium::Planetarium::draw(video::IVideoDriver*, core::rect<int>) + 764
  frame #7: terraverse`planetarium::Pixels::draw(...)
  frame #8: terraverse`shell::Planetarium::draw(video::IVideoDriver*) + 880
  frame #9: terraverse`shell::Menu::run(...) + 896
```

Once b165b8c8 is pushed, every master run's macOS job goes red at "Shell" (it doesn't block a
release: macOS isn't published yet).

**Reproduce.** Any machine whose OpenGL 3 context fails (macOS always): the menu, the planetarium
behind it. On Linux, forcing `video_driver = opengl` should show the same.

**Fix, either:**
- the legacy driver refuses or converts `EVT_PACKED` (or far tiles pick packed only when the driver
  is `EDT_OPENGL3`), so nothing reaches a driver that can't read it; and/or
- the real fix for macOS: on Apple, the GL3 driver asks for a 3.2+ *core*, forward-compatible
  context. That needs a bound VAO (the driver has none) and core-profile GLSL in every shader, so
  it's a port; the CI owner's macOS milestone needs it eventually and will take it up if nobody else
  does.

The engine's own crash report said nothing on macOS (the process died of SIGBUS with no `CRASH:`
lines); the backtrace above came from running the client under lldb. That's a second, smaller bug
for whoever owns `shared/platform/system/crash.cpp` (perhaps Chromium's handlers replace ours).
