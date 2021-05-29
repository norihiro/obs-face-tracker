# Face Tracker Properties

## Upsize recognized face

### Left, right, top, bottom
These properties upsize (or downsize) the recognized face by multiple of the width or height.

The motivation is that the face recognition returns a rectangle that is smaller than the actual face.

## Tracking target location

### Zoom
This property set the target zoom amount in multiple of the screen size.
If set to `1.0`, face size and screen size is same.
Smaller value results smaller face, i.e. less zoom.

### X, Y
This property set the location where the center of the face is placed.
`0` indicates the center, `+/-0.5` will result the center of the face is located at the edge.

### Scale max
This property set the maximum of the zoom.

## Tracking response

The tracking system has a PID control element + integrator.

### Kp
This is a proportional constant. The dimention is inverse time and the unit is s<sup>-1</sup>.
Larger value will result faster response.

### Ki
This is a integral constant. The dimention is inverse time and the unit is s<sup>-1</sup>.
Larger value results more tracking of slow move.

### Td
This is a derivative constant. The dimention is time and the unit is s.
0 will result no derivative term.
Larger value will make faster tracking when the subject start to move.

### LPF for Td
This is an inverse of the cut-off frequency for the low-pass filter (LPF), which affects the derivative term of PID control element. The dimention is time and the unit is s.
The LPF will reduce noise of face detection and small move of the subject.

## Debug
These properties enables how the face detection and tracking works.
Note that these features are automatically turned off when the source is displayed on the program of OBS Studio.
You can keep enable the checkboxes and keep monitoring the detection accuracy before the scene goes to the program.

### Show face detection results
If enabled, face detection and tracking results are shown.
The face detection results are displayed in blue boxes.
The Tracking results are displayed in green boxes.

### Stop tracking faces
If enabled, whole image will be displayed and yellow box shows how cropped.
This is useful to check how much margins are there around the cropped area.
