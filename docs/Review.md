# StepIt Camera Review

## Table of Contents <!-- omit in toc -->

- [Scope and Method](#scope-and-method)
- [Verdict](#verdict)
- [The Packages and Their Boundaries](#the-packages-and-their-boundaries)
- [Interfaces and Abstractions](#interfaces-and-abstractions)
- [Separation of Concerns](#separation-of-concerns)
- [SOLID](#solid)
- [One Request at a Time](#one-request-at-a-time)
  - [A Task That Timed Out Still Runs](#a-task-that-timed-out-still-runs)
  - [The Executor Waits with the Caller](#the-executor-waits-with-the-caller)
  - [The Errors of the Loop](#the-errors-of-the-loop)
- [The Path of a Picture](#the-path-of-a-picture)
- [The Web Server](#the-web-server)
- [The Test Page](#the-test-page)
- [Coupling and Duplication](#coupling-and-duplication)
- [Testability and Tests](#testability-and-tests)
- [Implementation Issues](#implementation-issues)
- [Recommendations](#recommendations)

Reviewed on 2026-11-15, at commit `11b679d`. It judges the design that [docs/ARCHITECTURE.md](ARCHITECTURE.md) and [docs/WEB_PAGE.md](WEB_PAGE.md) describe, as they stand in the working tree: both were refreshed after that commit and are not committed yet.

## Scope and Method

This review covers the whole module: the driver in [`src/stepit_camera`](../src/stepit_camera), 12 C++ files and about 2100 lines with the fake camera, its launch file and `camera.yaml`; the messages of [`src/stepit_camera_msgs`](../src/stepit_camera_msgs); the tests in [`src/stepit_camera_tests`](../src/stepit_camera_tests); and the test page in [`web`](../web), 1218 lines of TypeScript, judged against the patterns we use for our React pages. cpp-httplib, libgphoto2 and rosbridge are not reviewed, but were read or measured where the module depends on what they do: cpp-httplib 0.14.3, libgphoto2 2.5.31 and rosbridge 2.7.1, the versions of the image.

We read every file of the parts above. The module stands on its own, so this document names no project that uses it; where a finding matters to one, it says "a rig" or "a client". The deliberate choices of [Design Decisions and Trade-offs](ARCHITECTURE.md#design-decisions-and-trade-offs) are judged as design, not reported as bugs.

Each suspected bug of the driver, the node and the web server was reproduced with a throwaway probe, then deleted. The probes of the driver and of the node were gtest cases against a `Camera` that wraps `FakeCamera` and can be told to be slow or to fail, built with `stepit_camera` in a scratch workspace inside a container, on a ROS domain of its own; each asserted the right behaviour, so a finding marked **verified** failed its probe. The probes of the web server were `curl` requests to the real `web_server` executable. The probe of the page was a vitest case. `GPhotoCamera` has no automated test and no camera was touched: everything that depends on libgphoto2 or on a real camera is **from reading**. One suspicion was only partly reproduced, see finding 15.

The whole suite: `colcon test` of `stepit_camera_tests` passes 63 gtest cases in 5 programs, with no failure and no skip, in about 21 s, of which 10 s in the tests; `vitest run` of the page passes 26 cases in 4 files, with 1 skipped for want of a RAW sample, in 0.3 s, and its type check passes.

## Verdict

The design is sound, and its one idea, **the camera is a device that answers one request at a time**, holds where it matters most: only the thread of `CameraDriver` touches a `Camera`, no ROS callback reaches one directly, and the driver knows neither ROS nor libgphoto2. The interface `Camera` is the best seam of the module: 13 operations, two implementations that substitute for each other, and a fake good enough to carry 63 tests with no hardware. The packages keep to what they own and no include breaks the rule. The architecture documents say what the code does, including where it is weak.

The weakness is **what happens at the edges of that one thread**: a request that waits too long, an error in the middle of a turn, a disk that refuses a file. Each edge was written for the common case and loses something in the other:

- A request that times out is not withdrawn. The caller is told it failed, and it runs later: a test shot fires after its caller gave up (finding 1, verified), and a rejected setting reaches the camera (finding 2, verified).
- A picture is removed from the camera before it is saved, and an error on one file drops the files reported with it. With `keep_on_camera` false, both lose pictures (findings 3 and 4, verified).
- The web server sends the memory that follows a file when a range ends past it (finding 5, verified): a security issue on a server bound to every address.

None of these moves anything, so none is a safety issue in the sense of a motor. For a rig that shoots a focus stack of hundreds of pictures, a lost, stray or misplaced picture is the failure that matters, and findings 1, 3, 4, 6 and 7 can each cause one. None is hard to fix; the recommendations are small.

## The Packages and Their Boundaries

| Rule | Verdict | Evidence |
|---|---|---|
| `stepit_camera_msgs` holds no code | Kept | 3 messages and 1 service. |
| `stepit_camera` carries no test, `stepit_camera_tests` all of them | Kept | 5 test programs, all in `stepit_camera_tests`. |
| `Camera` and `CameraDriver` include the standard library only | Kept | [`camera.hpp`](../src/stepit_camera/include/stepit_camera/camera.hpp) and [`camera_driver.hpp`](../src/stepit_camera/include/stepit_camera/camera_driver.hpp) include nothing else. |
| `gphoto_camera.cpp` is the only file that includes libgphoto2 | Kept | One include of `<gphoto2/gphoto2.h>`; the header forward-declares `_Camera` and `_GPContext`. |
| Only `CameraNode` knows ROS | Kept | `rclcpp` is included by `camera_node.hpp` and by `web_server_main.cpp`, a `main()`. |
| `WebServer` knows neither the camera nor ROS | Kept | [`web_server.cpp`](../src/stepit_camera/src/web_server.cpp) includes cpp-httplib and its own headers. |
| Private dependencies stay out of the library's interface | Kept | `target_link_libraries` links libgphoto2, cpp-httplib and libjpeg `PRIVATE`. |
| The page's layers import downwards only | Kept | 25 relative imports, none upwards; `camera/store.ts` belongs to the top layer, as [The Layers](WEB_PAGE.md#the-layers) says. |
| No cycle | Kept | `camera_node` → `camera_driver` → `camera`; `gphoto_camera`, `fake_camera` → `camera`. |

Two remarks, neither a broken rule. `camera_node.hpp` includes `fake_camera.hpp`, which only `camera_node.cpp` needs. And the web server is built into the same shared library as the driver, so the camera node loads cpp-httplib's code and the web server loads libgphoto2: two libraries would cost a few lines of CMake and keep the two processes apart at link time as they are at run time.

## Interfaces and Abstractions

| Interface | Where | Judgement |
|---|---|---|
| `Camera` | [`camera.hpp`](../src/stepit_camera/include/stepit_camera/camera.hpp) | 13 operations, text values, one exception type. Nothing of libgphoto2 leaks. `waitForFiles()` returns a batch, and the interface says nothing of what a caller owes a file it could not download: that gap is finding 4. |
| `CameraError` and its `fatal` flag | same | One flag decides everything about errors, which is easy to follow. It is also all there is: "busy, try again" and "refused, do not try again" are the same error, so the loop cannot tell a file worth another try from one that is not. |
| `CameraDriver::run()` | [`camera_driver.hpp`](../src/stepit_camera/include/stepit_camera/camera_driver.hpp), line 122 | The right idea: a `std::packaged_task` brings the result and the exception back as if the caller had called the camera. Its timeout abandons the wait, not the task: see [A Task That Timed Out Still Runs](#a-task-that-timed-out-still-runs). |
| `CameraDriver::Callbacks` | same | Five `std::function`s keep ROS out of the driver. They run on the driver's thread, so `on_picture` makes the camera wait for the disk. `on_picture` returns nothing: the driver cannot learn that a picture was not saved, which is why it removes the file first (finding 3). |
| `DriverOptions` | same | Plain values, set by the tests to run in milliseconds. `max_preview_failures` and `poll_period` are not parameters, and `preview_rate` and `reconnect_period` are not checked (finding 13). |
| `SETTINGS` | [`settings.hpp`](../src/stepit_camera/include/stepit_camera/settings.hpp) | A new setting is one line, and the node, `get_settings` and the page follow with no other change: open to extension as promised. |
| The second constructor of `CameraNode` | [`camera_node.hpp`](../src/stepit_camera/include/stepit_camera/camera_node.hpp) | A test hands the node its camera. A clean seam, and the only one the node needs. |
| `~/picture` | [`Picture.msg`](../src/stepit_camera_msgs/msg/Picture.msg) | A path, never the content: the right call, well argued. The message tells nothing of the shot a file belongs to, so a client cannot tell the JPEG of a RAW+JPEG shot from the next shot, nor a test shot from a shot of the external device: it can only count. |
| `WebServer` | [`web_server.hpp`](../src/stepit_camera/include/stepit_camera/web_server.hpp) | Two folders and a port; cpp-httplib is hidden behind a forward declaration. It leaves the files of the mount point to cpp-httplib, with its defaults: see [The Web Server](#the-web-server). |

## Separation of Concerns

- **The driver decides what is deleted, the node whether it was saved.** `collectPictures()` removes a file from the camera (line 282), then `CameraNode::onPicture()` tries to save it. The one that knows the outcome is not the one that acts on it.
- **`CameraNode` is the composition root and the ROS interface at once**: it picks `FakeCamera` or `GPhotoCamera` (lines 163 to 171), declares the parameters, saves the files and matches the settings. At 480 lines it is still readable; saving a picture and applying a setting are already pure functions of `settings.cpp`, which is where the rest should follow.
- **`settings.cpp` holds five unrelated things**: the table of the settings, matching, the capture target, `~` and the folders, and writing a file. It is the module's utility drawer; it is tested well, and its name says less than it holds.
- **The page keeps its own idea of the live view**, `streaming` in the store, next to the driver's, on `~/status`. [The Live View](WEB_PAGE.md#the-live-view) says so and why. It is a copy all the same: a live view started by another client shows as off, and the page then offers Start where Stop is meant.

## SOLID

| Principle | Verdict | Evidence |
|---|---|---|
| Single responsibility | Mostly kept | `CameraDriver` runs the thread, `GPhotoCamera` speaks libgphoto2, `WebServer` serves files. `CameraNode` and `settings.cpp` each hold several, see above. |
| Open/closed | Kept | A setting is a line of `SETTINGS`; another way to reach a camera is another `Camera`. |
| Liskov substitution | Kept, with one difference | `FakeCamera` stands in for `GPhotoCamera` everywhere. It is thread safe where a real camera is not, by design, so a test cannot catch a call from the wrong thread; and it throws only `CameraError`, where `GPhotoCamera::setSetting()` can throw `std::invalid_argument` (finding 10). |
| Interface segregation | Kept | `Camera` is one interface of 13 operations, all used by its one client, the driver and the tasks it runs. |
| Dependency inversion | Kept in C++, not in the page | The driver depends on `Camera`, the node gets its camera through a constructor. In the page, `camera/store.ts` builds its `Camera` from module-level singletons, see [The Test Page](#the-test-page). |

## One Request at a Time

### A Task That Timed Out Still Runs

`run()` posts a task, waits 5 seconds, and throws "The camera did not answer in time" (line 138). The task stays in the queue, and `runTasks()` runs it when the thread is free. [One Thread Owns the Camera](ARCHITECTURE.md#one-thread-owns-the-camera) says so. What it costs depends on the task:

- **`~/take_picture`** answers `success` false, and the shutter is released later. The probe kept the thread in a download for 400 ms and called `run()` with a timeout of 100 ms: the caller got the `CameraError`, the shutter was released once all the same, and a second picture was downloaded. A client that was told the shot failed has no reason to expect that picture.
- **A setting** is rejected, and reaches the camera later. The probe made a download last 6.5 s and set `iso` to 800 meanwhile: `set_parameters` answered after 5001 ms with `successful` false, and afterwards `get_settings` read 800 on the camera while the parameter `iso` was still empty. `wanted_settings_` keeps the old value too (line 365 is not reached), so the next reconnection puts the camera back, silently.
- **`~/get_settings`** and **`~/stop_streaming`** only waste the camera's time later; `stopStreaming()` is harmless twice.

How likely is a 5-second wait? The thread downloads a file and the node writes it to disk before the next task runs, and a RAW+JPEG shot is two files in one turn. A 30 MB RAW over USB 2 and onto an SD card is a few seconds. The page already works around it: it stops reading the settings during its own test shot, "a request would only time out" ([`store.ts`](../web/src/client/camera/store.ts), line 187). A client that does not know when the shots are fired cannot.

### The Executor Waits with the Caller

`main.cpp` spins the node on one thread, and every service waits in `run()` on it. While one waits, nothing else of the node runs: not the timer of `~/status`, not `set_parameters` for `folder`, which needs no camera at all. The same probe measured the longest silence of `~/status` during the rejected `set_parameters`: 5001 ms, for a topic whose contract is "every second, so that a client that stops receiving it knows the node has stopped". A client whose patience is shorter than 5 s shows the camera as gone while it is downloading.

The comment of `main.cpp`, "the callbacks only wait for it when a setting changes", is out of date: `get_settings`, `take_picture` and `stop_streaming` wait too.

### The Errors of the Loop

`loop()` catches `CameraError` and nothing else (line 104). Three edges:

- **A non-fatal error ends the turn at once, and the next turn starts at once.** When the error repeats, e.g. a camera that answers every event query with an error that is not in the fatal list, nothing waits: the probe made `waitForFiles()` fail and counted 25362 warnings in 200 ms, each a log line. From reading, `GP_ERROR_TIMEOUT` and `GP_ERROR_CAMERA_ERROR` are not fatal for `isFatal()` in [`gphoto_camera.cpp`](../src/stepit_camera/src/gphoto_camera.cpp), line 44, so a camera that hangs rather than disappears may never be reconnected. Which errors a real camera repeats needs a camera to tell.
- **Five failed frames are a lost connection.** At 10 frames per second that is half a second. The probe made every frame fail with a non-fatal "busy": at 50 frames per second the driver closed the camera after 88 ms. From reading, a camera is busy for longer than half a second during a long exposure with the live view on, and closing it then, with the pictures in its memory only, loses the picture being taken. The fake does not show it, because its live view comes back as soon as `stopPreview()` is called.
- **Any other exception ends the process.** The probe threw `std::invalid_argument` from `on_connected`: `std::terminate`. That is what `std::stoi` and `std::stof` throw in `GPhotoCamera::setSetting()` (lines 242 and 248) for a toggle or a range widget given text; `onSetParameters()` does not catch it either (line 357), on the executor's thread. The five settings of a Canon EOS are lists, so it needs another brand to happen.

## The Path of a Picture

The driver downloads a file, removes it from the camera unless `keep_on_camera`, and hands it to the node, which saves and publishes it ([`camera_driver.cpp`](../src/stepit_camera/src/camera_driver.cpp), lines 271 to 298). Four things can go wrong on the way, and with `keep_on_camera` false, the setting for long series, each loses a picture:

- **An error on one file drops the rest of the batch.** `download()` throws out of the `for` loop, and the files reported in the same batch are forgotten: `waitForFiles()` does not report them again. The probe took a RAW+JPEG shot and made the download of the CR2 fail once, not fatally: no picture was published, both files stayed on the camera, and the next shot brought only its own file. From reading, the same holds inside `GPhotoCamera::waitForFiles()`: an error of `gp_camera_wait_for_event` after a file was collected throws the collected files away (line 285). In the camera's memory, the files are lost when the camera is closed or switched off, and until then they fill it.
- **A file is removed before it is saved.** The probe pointed `download_directory` into a folder that could not be written: nothing was published, and the camera held no file. The log says "it is lost", so this is known; [Downloading the Pictures](ARCHITECTURE.md#downloading-the-pictures) says it too. It is not a price of `keep_on_camera` false, though: removing the file after `on_picture` reports it saved costs nothing and loses nothing.
- **A failed save leaves a part of the file under its final name.** `savePicture()` opens the final path and writes ([`settings.cpp`](../src/stepit_camera/src/settings.cpp), line 145). The probe limited the file size to 40 kB and saved 100 kB: it threw, and `IMG_0001.CR2` stayed, 40000 bytes long; the next `IMG_0001.CR2` was saved as `IMG_0001_1.CR2`. The stream is checked before it is closed; libstdc++ writes anything over 1 kB straight through, so a failed write is seen, and only an error at `close()` would pass.
- **A file is readable while it is written.** The web server lists and serves it from its first byte. `~/picture` is published after `savePicture()` returns, when the file is closed, so a client that waits for the message always reads a whole file; one that polls the listing does not. The listing leaves out hidden files "so that a program writing a file under a temporary name [...] never shows it half written" ([The Web Server](ARCHITECTURE.md#the-web-server)): the driver itself is the one program that writes there, and it does not write that way.

**A picture takes the folder of the moment it is saved, not of the shot.** This is documented, and simple. Its price: a client that sets `folder` right after the first file of a RAW+JPEG shot gets the second file in the next folder. A client must wait for every file of a shot before it changes the folder, and nothing in `~/picture` tells it how many there are.

A multi-parameter change shows the same kind of edge in the node. `set_parameters_atomically` passes several parameters to `onSetParameters()` in one call, which applies each as it goes and returns at the first refusal (lines 320 to 366). The probe sent `folder: angle_02`, `iso: 800` and `aperture: 99` together: the call was refused for the aperture, the camera had ISO 800, the parameters `iso` and `folder` were unchanged, and the next picture went to `angle_02/IMG_0001.JPG`. `set_parameters`, which the page and `ros2 param set` use, calls the callback once per parameter and is not affected.

## The Web Server

The server binds `0.0.0.0` and allows every origin, on purpose, so that a page served elsewhere can load the pictures. What it serves is then open to the whole network, and to any page open in a browser on it, so its edges matter. The listing checks its paths with care (`folderUnder()`); the files are left to cpp-httplib's mount point (line 214), whose defaults are weaker. All verified with `curl` against the real executable:

| Request | Answer | Judgement |
|---|---|---|
| `/pictures/../../etc/hostname`, `%2e%2e`, `..%2f` | 404 | Refused, by cpp-httplib. |
| `Range: bytes=0-65535` of a file of 100 bytes | 206, `Content-Range: bytes 0-65535/100`, 65536 bytes | **The server sends what follows the file in its memory**: finding 5. |
| `Range: bytes=500000-600000` of a file of 200000 bytes | 206, 100001 bytes | The same, wholly outside the file. |
| `Range: bytes=0-100000000` of the same file | 206, then the connection closes after 6094617 bytes | The same; the server survives. |
| `/pictures/.secret.txt`, `/pictures/.hidden/in.txt` | 200 | Hidden files are served, although the listing leaves them out and refuses a hidden folder. |
| `/pictures/link.txt`, `/pictures/linkdir/out.txt`, links to a folder outside | 200, the file outside | Links are followed for files, although the listing refuses `linkdir/`. `ServesNothingOutsideItsFolders` tests `..` only. |
| `/pictures/withindex/`, a folder holding `index.html` | 200, the HTML | cpp-httplib serves `index.html` before the listing handler is asked. |
| `OPTIONS /pictures/small.jpg` | 404 | A browser that sends a preflight for a `Range` header from another origin is refused; current browsers send none for a simple range. |
| `/`, with no page built | 404, "The test page is not built", with the path of `web_directory` | As documented. |

The range is the serious one. cpp-httplib 0.14.3 maps the file and does not check a range against its size. The bytes past the end of a 100-byte file held 9077 non-zero bytes, with symbol names of the libraries mapped next to it; a wider range returned 4 MB of them. The process holds no secret of its own, only its parameters and its environment, and it is not the camera node: running the server as a process of its own, a decision of the design, is what keeps this away from the camera. It is still a read of the server's memory by anyone on the network, and the cause of the page's failure on small files, see below. Later versions of cpp-httplib answer 416.

The links and the hidden files matter less: whoever can write a link into `download_directory` already has the machine. They do make the test's name, and the document's "nothing served outside the two folders", say more than is true.

## The Test Page

The page is small, 1218 lines, and its layers are real: `ros/` knows no camera, `camera/camera.ts`, `picture.ts` and `raw.ts` know no React and are tested without a browser. Against our patterns for React pages:

| Pattern | Verdict | Evidence |
|---|---|---|
| One store, the single source of truth | Kept for the page, a copy for the live view | `useCamera` holds the section's state. `streaming` copies what the driver publishes on `~/status`, which the page does not read. |
| Derive, never copy | One slip | `LiveView.tsx` resets `failed` with an effect on `[src, streaming]` (line 24): a `key` on the `<img>` would do it. |
| Services injected, one composition root | Not kept | `camera()` in `store.ts` builds a `Camera` from `rosbridge()`, a module-level `Map` of connections, and from `useSettings`; `settings.ts` reads `window.matchMedia` when imported. So `store.ts`, the one file with rules, cannot be loaded by a test, as [Tests](WEB_PAGE.md#tests) admits. |
| Rules in a view model, not in the view | Mostly kept | The store is the view model. One rule is in the view: only the `disabled` of the button in `TestShot.tsx` stops a second `takeShot()` while one runs; the store does not refuse it. |
| Effects at the edges only | Kept but for the slip above | `CameraPage` mounts `followCamera()` with its cleanup. |
| Stale work is dropped | Kept for the shot | The counter `shots` drops a picture that loads after the next shot started. A `refresh()` sent before a change can still answer after it and show the old value until the next one. |
| Progress and cancellation | Not needed, not there | No `AbortController`: a picture that loads is not cancelled, only dropped. |
| Errors as values | Not kept, contained | `Camera` and `Rosbridge` throw; the store turns every error into state, so no component sees an exception. |
| Storage keys in one file | Not kept | `stepit.settings` in `settings.ts`, `stepit.section` in `App.tsx`. |

Three behaviours are worth more than the patterns:

- **The page cannot find a picture saved in a folder.** `Camera.pictureUrl()` keeps only the file name of `path` (line 107), and ignores `relative_path`. The vitest probe: for `…/pictures/2026-10-06/angle_01/IMG_0042.JPG` it gives `/pictures/IMG_0042.JPG`, a 404. [The Test Shot](WEB_PAGE.md#the-test-shot) says so. The driver publishes `relative_path` for exactly this, and the page that tests the driver is the one client that does not use it: with `folder` set, by anyone, the test shot shows a broken picture.
- **`loadPicture()` asks for the first 64 KB of every file, a JPEG included, before looking at its name** ([`picture.ts`](../web/src/client/camera/picture.ts), line 21). For a file shorter than 64 KB the range ends past it, and the server answers as in [The Web Server](#the-web-server): with luck 64 KB of which the page uses none, without it a connection cut short and a picture that "cannot be loaded". The fake camera's pictures are larger, which is why no test shot shows it.
- **The subscription to `~/picture` asks rosbridge for `queue_length: 1`** ([`rosbridge.ts`](../web/src/client/ros/rosbridge.ts), line 202), "a page that falls behind skips to the latest message". From reading rosbridge 2.7.1, any `queue_length` above 0 puts a queue of that length between the topic and the socket, so of two pictures published close together the first can be dropped. It is the right default for a status, and the wrong one for a topic where each message is a file; `subscribe()` has no way to say which.

## Coupling and Duplication

- **`ros/` and `camera/` are written to be reused, and ship as source to copy.** [WEB_PAGE.md](WEB_PAGE.md#introduction) says they "can be reused by another web application". With no package to import, reuse is a copy, and a copy diverges: an application that copied them has to fix what it meets, e.g. the range past the end or the folder of a picture, in its copy, and the fix does not come back here. Both fixes above exist today outside this repository and not in it.
- **`download_directory` is one parameter read by two processes.** `camera.yaml` gives it to `/**`, so the node and the web server agree as long as both read the same files; a parameter set on one node only, e.g. on the command line, makes the server serve another folder than the node fills, with no check and no warning.
- **The routes are written three times**: in `WebServer`'s constructor, in `openapi.json`, and in the list of `DescribesItsApi`. The test ties the last two; nothing ties the first to them. The documents say so.
- **The page's defaults repeat the launch file's**: ports 9091 and 8081 in `settings.ts`, `/camera` as the node, 64 KB in `picture.ts` and in the documents. Kept in step by hand, 4 values.
- **`Picture.msg` says two things about the address of a picture**: its header comment, "load it from the web server [...] at `/pictures/<file name of path>`", is true only while `folder` is empty; the comment of `relative_path`, 20 lines below, gives the right one.

## Testability and Tests

What is tested is tested well: the fake behaves like the camera where the driver depends on it, the tests wait on conditions, not on time, each node test runs in a namespace of its own, and the web server is tested over real HTTP.

What is not tested is listed honestly in [Tests](ARCHITECTURE.md#tests): the limits of the loop, a picture that cannot be saved, the launch file. Every verified finding of this review is in that list or next to it. The reason is one missing seam: **`FakeCamera` cannot be told to fail or to be slow**. It can only be unplugged, a fatal error. A camera that is busy, that fails one download, or that takes 6 seconds needs a wrapper written in each test; the probes of this review needed one of 60 lines, and with it each finding was a test of 10 to 20 lines.

| Part | Tests | What is missing |
|---|---|---|
| `CameraDriver` | 14 | A task that outlasts its timeout; a non-fatal error in a download, in `waitForFiles()` and in a frame; an exception that is not a `CameraError`. |
| `CameraNode` | 15 | A picture that cannot be saved; several parameters in one call; a request while the thread is busy. |
| `settings` | 12 | A write that fails. |
| `WebServer` | 13 | A range past the end, a hidden file, a link to a file. |
| `GPhotoCamera` | 0 | Everything: 318 lines checked by hand. `isFatal()` and the parsing of a value by widget type are pure and could be tested without a camera if they were free functions. |
| The page's `store.ts`, `connection.ts`, `settings.ts`, components | 0 | The sequence of a test shot, 60 lines of asynchronous code, is the part of the page most likely to break. |

## Implementation Issues

Ordered by severity. **Safety** says whether the issue can lose, add or misplace a picture of a series, the worst a camera can do to a rig: it moves nothing.

1. **A test shot that timed out fires later.** Verified. See [A Task That Timed Out Still Runs](#a-task-that-timed-out-still-runs). `~/take_picture` answers `success` false, the shutter is released when the thread is free, and the picture is published like any other. **Safety: yes**, a stray picture, which a client that counts pictures takes for the next shot's.
2. **A setting rejected for a timeout reaches the camera.** Verified. Same place. The camera has the new value, the parameter and `wanted_settings_` the old one, and the next reconnection restores the old one. **Safety: indirectly**: the exposure changes in the middle of a series after the client was told it did not.
3. **A picture that cannot be saved is deleted from the camera first.** Verified. See [The Path of a Picture](#the-path-of-a-picture). Only with `keep_on_camera` false. **Safety: yes**, a lost picture; the client sees that none came.
4. **An error on one file drops the files reported with it.** Verified in the driver, from reading in `GPhotoCamera::waitForFiles()`. Same place. With `keep_on_camera` false the files stay in the camera's memory and are never fetched. **Safety: yes**, a lost picture, e.g. the JPEG of a RAW+JPEG shot whose RAW was refused once.
5. **A range that ends past a file sends the server's memory.** Verified. See [The Web Server](#the-web-server). **A security issue**: a read of the memory of `web_server` by any client on the network. **Safety: no.**
6. **A failed save leaves a truncated file under the picture's name.** Verified. See [The Path of a Picture](#the-path-of-a-picture). The listing shows it, and a stacking program that reads the folder takes it for a picture. **Safety: yes**, a damaged picture in a series, and the next picture of that name is renamed.
7. **A refused change of several parameters is half applied.** Verified. Same place. Only through `set_parameters_atomically`. **Safety: yes** for a client that uses it with `folder`: the next pictures go into a folder the parameter does not name.
8. **Five failed frames close the camera.** Verified in the driver, from reading for a real camera. See [The Errors of the Loop](#the-errors-of-the-loop). **Safety: possibly**, with the live view on during a long exposure and `keep_on_camera` false: the picture being taken is in the memory of a camera that is closed.
9. **The node is deaf for up to 5 seconds while a request waits.** Verified. See [The Executor Waits with the Caller](#the-executor-waits-with-the-caller). `~/status` was silent for 5001 ms, and `folder` cannot be set meanwhile. **Safety: no**, as long as a client waits for the answer of `set_parameters` before the next shot.
10. **An exception that is not a `CameraError` ends the process.** Verified for `on_connected`, from reading for `GPhotoCamera::setSetting()` and `onSetParameters()`. See [The Errors of the Loop](#the-errors-of-the-loop). Not reachable with the five settings of a Canon EOS. **Safety: no.**
11. **A non-fatal error that repeats spins the loop.** Verified in the driver, from reading for which errors a camera repeats. Same place: 25362 warnings in 200 ms, a core and a log file. **Safety: no**; the camera is not reconnected either.
12. **The mount point serves hidden files, links to files outside, and `index.html` in place of a listing.** Verified. See [The Web Server](#the-web-server). **A minor security issue**: it needs write access to the folder. **Safety: no.**
13. **`preview_rate` and `reconnect_period` are not checked.** Verified for `preview_rate`: at 0, the period is infinite, its conversion overflows, and the probe got 27 frames in 500 ms, as fast as the fake encodes them. From reading, `reconnect_period` 0 or negative opens the camera in a tight loop. **Safety: no.**
14. **The page: no picture in a folder, a range past the end of a small file, and a queue of one for the pictures.** The first verified, the second verified on the server's side, the third from reading. See [The Test Page](#the-test-page). **Safety: no**: the page only shows.
15. **The constructor of `CameraNode` can wait for the camera, and throw.** Partly reproduced. `setStreaming(stream_on_start)` (line 186) calls `stopStreaming()`, which waits in `run()` when the camera is already connected. With a camera that opens at once and takes 1.2 s to configure, the constructor returned in 3 to 12 ms in 3 runs of 4, and waited 1205 ms in the fourth; it did not throw, since nothing lasted 5 s. From reading, it throws only if the camera opens within microseconds and `onConnected` then outlasts 5 seconds: a real camera takes far longer to open, so the constructor has passed by then. **Safety: no.**
16. **Smaller points, from reading.** `dataOf()` ignores the result of `gp_file_get_data_and_size` (line 75): a failure would save an empty file as a picture. `savePicture()` checks the stream before closing it. `stopStreaming()` clears the flag even when the task then times out, so `~/status` says `streaming` false while the camera may still be in live view for a turn. The comment of `main.cpp` and the header comment of `Picture.msg` say other than the code, see above. The module's `CLAUDE.md` asks every source file for the MIT header; the 18 TypeScript files of `web/src/client` carry none.

## Recommendations

Ordered by value against effort; each fits one pull request.

1. **Withdraw a task whose caller gave up.** `run()` shares a flag with its task and sets it on timeout; `runTasks()` skips a task whose flag is set. Fixes findings 1 and 2. The trade-off: a task that already started cannot be withdrawn, so `run()` should say which of the two happened, "not started" or "still running".
2. **Remove a file only once it is saved, and keep the files that failed.** `on_picture` returns whether the picture was saved; `collectPictures()` catches a non-fatal error per file, keeps the file in a list and tries it again on the next turns, a few times, then warns once. Fixes findings 3 and 4. The trade-off: with `keep_on_camera` false, a file that never downloads stays in the camera's memory, which is where it is today.
3. **Write a picture under a hidden temporary name, then rename it.** `savePicture()` writes `.<name>.part`, closes, checks, renames, and removes the part on failure. Fixes finding 6 and the half-written file in the listing, and makes true what the listing already assumes. A test with a file size limit, as the probe did.
4. **Check every file request before cpp-httplib serves it.** A pre-routing handler for `/pictures/` that answers 416 to a range starting past the end of the file, cuts one that ends past it, and refuses hidden parts and paths whose canonical form leaves the folder, with the code `folderUnder()` already has; or a cpp-httplib that checks ranges. Fixes findings 5 and 12. The trade-off: a `stat()` per request, which the listing already pays.
5. **Give `FakeCamera` failures to order**, e.g. `failNext(operation, error)` and `setDelay(operation, duration)`, and write the tests the list of "no automated test" names. Fixes nothing by itself; every other recommendation needs it to be tested, and the probes of this review show each is 10 to 20 lines.
6. **Keep `~/status` and `folder` away from the camera's waits.** The status timer and the parameters in a callback group of their own on a `MultiThreadedExecutor` of 2 threads. Fixes finding 9. The trade-off: two executor threads in the node; `status_`, `folder_` and `wanted_settings_` are already under their own locks, the rest must be checked.
7. **Check a change of parameters whole before applying any of it.** One task that matches every value against the camera's choices, then sets them; `folder_` and `wanted_settings_` change only after it succeeded. Fixes finding 7.
8. **Make the loop patient and its errors finite.** After a non-fatal error, wait `poll_period`; count the failures of the live view in time, longer than the longest exposure, not in frames; treat a non-fatal error that repeats for some seconds as a lost connection; catch `std::exception` in the loop and in `onSetParameters()`, and turn the errors of `std::stoi` and `std::stof` into `CameraError`; refuse a `preview_rate` or a `reconnect_period` that is not positive. Fixes findings 8, 10, 11 and 13. The time limits need a real camera to choose.
9. **Make the page use what the driver gives it.** `pictureUrl()` from `relative_path`; no range for a JPEG; `queue_length` chosen by the caller of `subscribe()`, none for `~/picture`; `streaming` read from `~/status`. Fixes finding 14 and the copy of the live view's state. Then give `store.ts` its `Camera` and its settings from outside, so that the sequence of a test shot gets a test.
10. **Publish `ros/` and `camera/` as a package**, or name one repository as their home, so that a fix made by an application that uses them has a place to come back to. The trade-off: a package to version for about 600 lines.
