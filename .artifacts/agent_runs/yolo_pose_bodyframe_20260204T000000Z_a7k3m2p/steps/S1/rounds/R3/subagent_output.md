{"type":"thread.started","thread_id":"019c2708-3361-7bb3-a5ec-296bc9fa32e5"}
.
item_0:**Planning 3D skeleton depth processing**

I'll implement depth median filtering per keypoint like the hip, convert to camera coordinates, then transform to trampoline frame using depth region rotation. I'll extend KeyPoint minimally with separate 3D fields for camera and trampoline coordinates and compute per-frame body rotation from these points.
item_1:**Adding rotation matrix getter and computing relative rotation**

I'll add a getter for the private rotation matrix in depth region and compute the relative rotation between body and bed frames as defined. Then I'll derive per-axis rotation angles and cumulative rotations from this relative rotation to track flip, twist, and side-flip counts.
.