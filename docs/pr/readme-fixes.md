# The READMEs Checked Against the Code

Branch `readme-fixes`.

## Goal

Every statement of `README.md`, `docker/README.md` and `docs/pr/README.md` was read against the code, the launch file, `camera.yaml` and the other documents. Twelve were wrong or said more than is true. This pull request corrects them.

It changes documents only. No code, no launch file and no configuration changes.

## Decisions

**The live view and a shot: only what is known.** The README said that a camera in live view ignores a shot, and elsewhere that a test shot works during the live view. It now says the three things we know: a 5D Mark II ignores the shutter button on its body, a test shot over USB works and pauses the live view, and the remote shutter release cable is not tested yet. `docs/ARCHITECTURE.md` had the same sentence, and gets the same correction.

**The README says what happens today.** Where it described something the code does not do, it now says what the code does, in one sentence, and points to `docs/Review.md`:

- the test page shows a test shot only while `folder` is empty;
- the web server serves a hidden file asked by its path, and a link to a file outside;
- a folder that holds an `index.html` gives that file, not its listing;
- a web server started before `build` does not find the page: the driver must start again.

**The default of `download_directory` stays `~/ws/pictures`**, the value of `camera.yaml`, as for the other parameters of the table. Its description adds that the node and the web server use `~/pictures` without that file.

**Smaller corrections.** A setting is checked only while the camera is connected. `streaming` on `/camera/status` is what was asked for, not what the camera does. The workflow of the test page runs only when `web` changes. `dock.sh clean` removes the image too. `dock.sh` does not choose the folder to mount: the compose file does.

## Left Out

- The sentences about real cameras that the code cannot prove, e.g. what each position of the mode dial allows: they stay as they are.
- `docs/pr/018-stop-streaming-waits.md` says that a camera in live view ignores a shot. A note is not rewritten after the merge.
- The same kind of slip outside the READMEs, e.g. the header comment of `Picture.msg` and `/pictures/<name>` in `CLAUDE.md`.

## Open Items

- Whether the remote shutter release cable fires a camera in live view: try it with a camera.
- `ros2 run rqt_image_view rqt_image_view /camera/preview/compressed`, in the README, was not run: check that it shows the frames.
- The fixes of the code behind the sentences above are in the recommendations of `docs/Review.md`.
