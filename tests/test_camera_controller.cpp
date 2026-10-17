#include <cassert>
#include <cstdint>

#include <apps/app_media_camera/camera_controller.h>

using app_media_camera::CameraCallbacks;
using app_media_camera::CameraController;
using State = hal::HalBase::CameraState;

int main()
{
    int starts           = 0;
    int stops            = 0;
    bool acceptStart     = true;
    lv_obj_t* lastCanvas = nullptr;
    hal::HalBase::CameraStatus snapshot{State::Stopped};

    CameraCallbacks callbacks;
    callbacks.start = [&](lv_obj_t* canvas) {
        ++starts;
        lastCanvas = canvas;
        if (!acceptStart) {
            snapshot.state     = State::Failed;
            snapshot.lastError = "Could not start camera task";
            return false;
        }
        snapshot       = {};
        snapshot.state = State::Starting;
        return true;
    };
    callbacks.stop = [&] {
        ++stops;
        snapshot.state = State::Stopping;
    };
    callbacks.status = [&] { return snapshot; };

    CameraController controller(callbacks);
    assert(controller.statusText() == "Camera stopped");
    assert(!controller.isRunning());
    assert(controller.canStart());
    assert(!controller.canStop());
    assert(!controller.start(nullptr));
    assert(starts == 0);

    auto canvas = reinterpret_cast<lv_obj_t*>(static_cast<uintptr_t>(0x1234));
    assert(controller.start(canvas));
    assert(starts == 1 && lastCanvas == canvas);
    assert(!controller.isRunning());
    assert(!controller.canStart());
    assert(controller.canStop());
    assert(controller.statusText() == "Starting camera");
    assert(!controller.start(canvas));
    assert(starts == 1);

    snapshot.state = State::Running;
    assert(controller.isRunning());
    assert(controller.statusText() == "Camera running");
    controller.stop();
    assert(stops == 1);
    assert(!controller.isRunning());
    assert(!controller.canStart() && !controller.canStop());
    assert(controller.statusText() == "Stopping camera");
    assert(!controller.start(canvas));
    controller.stop();
    assert(stops == 1);

    snapshot.state = State::Stopped;
    assert(controller.canStart());
    acceptStart = false;
    assert(!controller.start(canvas));
    assert(!controller.isRunning());
    assert(controller.statusText() == "Camera failed");
    assert(controller.status().lastError == "Could not start camera task");
    assert(controller.canStart());

    acceptStart = true;
    assert(controller.start(canvas));
    snapshot.state     = State::Failed;
    snapshot.lastError = "Camera frame timeout";
    assert(!controller.isRunning());
    assert(controller.status().lastError == "Camera frame timeout");
    // Close must detach the target even after a spontaneous worker failure.
    controller.stopOnClose();
    assert(stops == 2);

    snapshot.state = State::Stopped;
    assert(controller.start(canvas));
    controller.stopOnClose();
    assert(stops == 3);

    snapshot.state = State::Unavailable;
    assert(!controller.canStart() && !controller.canStop());
    assert(!controller.start(canvas));
    assert(controller.statusText() == "Camera unavailable");
    CameraController unavailable({});
    assert(!unavailable.start(canvas));
    assert(unavailable.status().state == State::Unavailable);
    unavailable.stopOnClose();
    return 0;
}
