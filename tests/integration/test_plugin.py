"""Covers that OBS loads the plugin, with the filters and sources it
registers, and the plugin of test sources beside it, with the test video
source the other tests show a face with.
"""

from conftest import TRACKER_KIND, VIDEO_SOURCE_KIND


def test_the_plugin_has_its_filters(world):
    kinds = world.ws.call("GetSourceFilterKindList")["sourceFilterKinds"]
    assert {"face_tracker_filter", TRACKER_KIND} <= set(kinds)


def test_the_plugin_has_its_sources(world):
    kinds = world.ws.call("GetInputKindList")["inputKinds"]
    assert "face_tracker_source" in kinds


def test_the_test_sources_are_loaded(world):
    kinds = world.ws.call("GetInputKindList")["inputKinds"]
    assert VIDEO_SOURCE_KIND in kinds
