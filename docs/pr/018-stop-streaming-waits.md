# Stopping the Live View Waits for the Camera

Pull request [#18](https://github.com/kineticsystem/stepit-camera/pull/18), branch `stop-streaming-waits`.

## Goal

A camera in live view ignores a shot. A client that stops the live view to shoot must know when the camera has left it, but `~/stop_streaming` answered as soon as it had set a flag, before the driver's loop switched the live view off.

## Decisions

**`CameraDriver::stopStreaming()`.** It clears the flag, then switches the live view off on the driver's thread, through `run()`, and returns once that is done. `previewing_` belongs to that thread, so nothing else touches it.

**`~/stop_streaming` uses it.** The service answers once the camera is ready to shoot, or fails if the camera does not leave the live view in time. The status tells `streaming` false at once, as before.

**A disconnected camera only clears the flag,** as `setStreaming(false)` did: there is no live view to leave.
