# Face Tracker PTZ Properties

### Reset tracking (button)
When clicked, tracking state is reset to the initial condition; reset internal states of the integrators, send reset command to the PTZ device.
(This is not a property.)

## Preset

### Preset
This combo-box sets the name of the preset to be loaded or saved.

### Load preset
To load the preset, select the preset from `Preset` combo-box and click `Load preset` button.

### Save preset
To save current propeties as a preset, type preset name in `Preset` combo-box and click `Save preset` button.

### Delete preset
To delete awn existing preset, select the preset from `Preset` combo-box and click `Delete preset` button.

### Save and load tracking parameters
If you want to save only tracking parameters (`Upsize recognized face` and `Tracking target location`), enable only this check-box.

### Save and load control parameters
If you want to save only contol parameters (`Tracking response`), enable only this check-box.

## Face detection options

### Left, right, top, bottom
These properties upsize (or downsize) the recognized face by multiple of the width or height.

The motivation is that the face recognition returns a rectangle that is smaller than the actual face.

### Scale image
The frame will be scaled before sending into face detection and tracking algorithm.
Default is `2`.
Larger value will reduce CPU usage but too large value will fail to detect faces.
The face detection engine requires size of the faces at least 80x80.
If you have low resolution image, it is highly recommended to set to `1`.

### Tracking threshold
This property sets the threshold when to stop tracking after the face is lost.
During correlation tracking, scores are accumulated.
When the score drops lower than the specified threshold,
the tracking will be stopped.

## Tracking target location

### Zoom
This property set the target zoom amount in multiple of the screen size.
If set to `1.0`, face size and screen size is same.
Smaller value results smaller face, i.e. less zoom.

### X, Y
This property set the location where the center of the face is placed.
`0` indicates the center, `+/-0.5` will result the center of the face is located at the edge.

## Tracking response

The tracking system has a PID control element + integrator.

### Kp (Pan, Tilt, Zoom)
This is a proportional constant in decibel.
Larger value will result faster response.
Since the gain of the PTZ camera depends on the manufactures and models,
you need to adjust Kp for your camera.

### Ki
This is a integral constant. The dimension is inverse time and the unit is s<sup>-1</sup>.
Larger value results more tracking of slow move.

### Td
This is a derivative constant. The dimension is time and the unit is s.
0 will result no derivative term.
Larger value will make faster tracking when the subject start to move.

### LPF for Td
This is an inverse of the cut-off frequency for the low-pass filter (LPF), which affects the derivative term of PID control element. The dimension is time and the unit is s.
The LPF will reduce noise of face detection and small move of the subject.

### Dead band nonlinear band (X, Y, Z)
These parameters make dead bands and nonlinear bands for the error signal that goes to PID control element.
The unit is a percentage of the average of source width and height.
If the error signal is within the dead band, error signal is forced to zero to avoid small move to be tracked.
The nonlinear band makes smooth connection from the dead band to the linear range.

### Attenuation time for lost face
After the face is lost, integral term will be attenuated by this time. The dimension is time and the unit is s.

## Debug
These properties enables how the face detection and tracking works.
Note that these features are automatically turned off when the source is displayed on the program of OBS Studio.
You can keep enable the checkboxes and keep monitoring the detection accuracy before the scene goes to the program.

### Show face detection results
**Deprecated**
If enabled, face detection and tracking results are shown.
The face detection results are displayed in blue boxes.
The Tracking results are displayed in green boxes.

### Always show information
**Deprecated**
If enabled, debugging properties listed above are effective even if the source is displayed on the program.
This will be useful to make a demonstration of face-tracker itself.

## Output
This property group configure how to connect to the PTZ camera.

### PTZ Type
Specify the protocol to connect to the camera.
| Type | Description |
| ---- | ----------- |
| `None` | Do not connect to camera. Control message will be logged. |
| `through PTZ Controls` | Send through the PTZ Controls plugin. |
| `VISCA over TCP` | Send using TCP connection to the camera. |

The option `through PTZ Controls` requires the other plugin [PTZ Controls](https://github.com/glikely/obs-ptz).
The feature could be broken by future release of either plugin.

### IP address, port
The address and port of the camera you are connect to.
You can specify IP address or host name if your system can resolve it.

### Max control (pan, tilt, zoom)
These sliders can limit the maximum control amount to the camera.
If you want to disable changing zoom, set it to `0`.

### Invert control (pan, tilt, zoom)
These checkboxes invert the direction of the control.
It might be useful if you camera is mounted on ceil.

### Invert control (zoom)
Just in c ase the zoom control behave opposite directon, check this.
You should not check this in most cases.
This is a deplicated option.
