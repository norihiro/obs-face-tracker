'''
Test plugin is loaded
'''

import unittest
from onsdriver import obstest

PLUGIN_NAME = 'face-tracker'


class PluginLoadTest(obstest.OBSTest):
    'Test the plugin is loaded by OBS Studio'

    def test_log(self):
        'Check log file'
        search_text = f' [{PLUGIN_NAME}] '
        loaded = False
        with open(self.obs.get_logfile(), 'r', encoding='utf-8') as fr:
            for line in fr:
                if search_text in line:
                    print(line)
                    loaded = True
                    break
        self.assertTrue(loaded)


if __name__ == '__main__':
    unittest.main()
