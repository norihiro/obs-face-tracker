"""Covers the face tracker finding a face, in what its filters show: the Face
Tracker filter (src/face-tracker.cpp) framing it, and the Face Tracker PTZ
filter (src/face-tracker-ptz.cpp) marking it, which neither needs a camera.

The face is a photo shown by the test video source
(tests/test-sources/test-video-source.c), and what a filter shows is read from a screenshot
of its source.
Needs the face detector's model: see FACE_TRACKER_DATA in conftest.py.
"""

import pytest
from conftest import FACE_IMAGE, FRAME_SIZE, bounding_box, have_detector_model, is_marker, is_photo

pytestmark = pytest.mark.skipif(
    not have_detector_model() or not FACE_IMAGE.is_file(),
    reason="needs FACE_TRACKER_DATA with the face detector model, and dlib's example faces (submodules)")


def box_height(box):
    return box[3] - box[1]


def test_the_face_tracker_frames_a_face(world):
    """A face off the middle is shown larger, and nearer the middle. It is as
    large as it is so that the detector finds it at its default scale."""
    source = world.make_face_source((0.65, 0.5), face_height=0.5)
    shown = {}

    def showing():
        shown["box"] = bounding_box(world.screenshot(source), is_photo)
        return shown["box"] is not None

    # once its first frame is there
    world.wait_for(showing, timeout=10, interval=0.5)
    before = shown["box"]
    world.ws.call("CreateSourceFilter", {"sourceName": source, "filterName": world.unique("Face Tracker"),
                                         "filterKind": "face_tracker_filter", "filterSettings": {}})

    seen = {}

    def framed():
        seen["box"] = bounding_box(world.screenshot(source), is_photo)
        return seen["box"] is not None and box_height(seen["box"]) > 1.5 * box_height(before)

    try:
        world.wait_for(framed, timeout=30, interval=1.0)
    except AssertionError:
        raise AssertionError(f"the face was not framed: the photo was {before}, and is {seen.get('box')}")
    middle = FRAME_SIZE[0] / 2
    centre = (seen["box"][0] + seen["box"][2]) / 2
    assert abs(centre - middle) < abs((before[0] + before[2]) / 2 - middle)


def test_the_ptz_tracker_marks_a_face(world):
    """With its debug view on, the tracker draws a green box on the face it
    follows, which is on the photo, and about as big as a face on it is.
    It needs no device to do so: "dummy" is the PTZ type of None."""
    source = world.make_face_source((0.5, 0.5), face_height=0.5)
    tracker = world.make_tracker(source, **{"debug_faces": True, "debug_always_show": True, "ptz-type": "dummy"})

    seen = {}

    def marked():
        image = world.screenshot(source)
        seen["marker"] = bounding_box(image, is_marker)
        seen["photo"] = bounding_box(image, is_photo, thickness=5)
        return seen["marker"] is not None and seen["photo"] is not None

    try:
        world.wait_for(marked, timeout=30, interval=1.0)
    finally:
        tracker.remove()
    marker, photo = seen["marker"], seen["photo"]
    marker_x = (marker[0] + marker[2]) / 2
    assert photo[0] < marker_x < photo[2], (marker, photo)
    assert 0.3 < box_height(marker) / box_height(photo) < 0.9, (marker, photo)
