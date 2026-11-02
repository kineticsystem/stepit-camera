# The Status Tells Whether the Live View Is On

Pull request [#17](https://github.com/kineticsystem/stepit-camera/pull/17), branch `publish-streaming`.

## Goal

The live view is switched by `~/start_streaming` and `~/stop_streaming`, but nothing told a client whether it was on. A client that did not switch it, or one opened later, could only guess, and showed it wrong.

## Decisions

**`streaming` on `~/status`.** `CameraStatus` carries whether the live view is on. The status is already latched and published every second, so a client opened late gets it at once.

**Published at once.** `CameraNode::setStreaming()` sets the flag of the driver and publishes the status, for the two services and for `stream_on_start`.
