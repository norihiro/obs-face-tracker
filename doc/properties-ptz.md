# Face Tracker PTZ Properties

### Reset tracking (button)
When clicked, tracking state is reset to the initial condition; reset internal states of the integrators, send reset command to the PTZ device.
(This is not a property.)

## Preset
**Deprecated**
Preset will be provided through a dock. The preset group in the property dialog will be removed in the future release.

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

### Crop left, right, top, and bottom for detector
These properties crop the image before sending to the face detection algorithm.
The unit is pixel before scaling the image.

The properties won't affect tracking.
If the face is once detected and moved out from the cropped region,
the tracking will still continue.

### Landmark detection
Specify dataset for face landmark detection and enable the checkbox
to calculate location and size of the face.
The location is calculated by the average of all the landmark points for each face.
The size is calculated by the area surrounded by the landmark points.
You might need to adjust tracking target location and zoom depending on the landmark datasets.

A dataset file `shape_predictor_5_face_landmarks.dat` is bundled so that you can try it soon.
Original data is distributed at [dlib-models](https://github.com/davisking/dlib-models).
Another model `shape_predictor_68_face_landmarks.dat` is ready but not bundled due to a license incompatibility.

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

### Kp (X, Y, Z)
This is a proportional constant in decibel.
Larger value will result faster response.
Since the gain of the PTZ camera depends on the manufactures and models,
you need to adjust Kp for your camera.

### Ki (X, Y, Z)
This is an integral constant. The dimension is inverse time and the unit is s<sup>-1</sup>.
Larger value results in more tracking of slow movement.

### Td (X, Y, Z)
This is a derivative constant. The dimension is time and the unit is s.
0 will result in no derivative term.
Larger value will make tracking faster when the subject starts to move.

### LPF for Td (X, Y, Z)
This is an inverse of the cut-off frequency for the low-pass filter (LPF), which affects the derivative term of the PID control element. The dimension is time and the unit is s.
The LPF will reduce noise of face detection and small movement of the subject.
This property is shared for X and Y axises.

### Dead band nonlinear band (X, Y, Z)
These parameters make dead bands and nonlinear bands for the error signal that goes to PID control element.
The unit is a percentage of the average of source width and height.
If the error signal is within the dead band, error signal is forced to zero to avoid small move to be tracked.
The nonlinear band makes smooth connection from the dead band to the linear range.

### Attenuation time for lost face
After the face is lost, integral term will be attenuated by this time. The dimension is time and the unit is s.

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

### Save correlation tracker, calculated error, control data to file
**Not available for released version**
Save internal calculation into the specified file for each.
This option is not available without building with `ENABLE_DEBUG_DATA`
but still can be set through obs-websocket or manually editing the scene file to add a text property with a file name to be written.
To disable it back, remove the property or set zero-length text.

#### Correlation tracker
Property name: `debug_data_tracker`

The data contains time in second, 3 coordinates (X, Y, Z), and score of the correlation tracker.
The X and Y coordinates are the center of the face.
The Z coordinate is a square-root of the area.
Sometimes multiple correlation trackers run at the same time. In that case, multiple lines are written at the same timing.

#### Calculated error
Property name: `debug_data_error`

The data contains time in second, 3 coordinates (X, Y, Z).
The calculated error is the adjusted measure with current resolution, the cropped region when the frame was rendered, and user-specified tracking target.
0-value indicates the face is well aligned and positive or negative value indicates the cropped region need to be moved.

#### Control
Property name: `debug_data_control`

The data contains time in second, 3 coordinates (X, Y, Z), and another set of 3 coordinates.
The first set of the coordinates is a linear floating-point value of the control signal.
The second set of the coordinates is an integer value that should go to the PTZ device.
