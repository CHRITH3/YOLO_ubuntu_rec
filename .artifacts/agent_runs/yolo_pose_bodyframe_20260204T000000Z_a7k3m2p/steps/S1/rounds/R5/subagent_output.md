{"type":"thread.started","thread_id":"019c2708-3361-7bb3-a5ec-296bc9fa32e5"}
.
item_0:**Planning 3D skeleton computation**

I'm outlining access to rotation matrix and origin from the depth region to transform poses into trampoline coordinates, adding getters in DepthRegion as needed. Next, I'll compute 3D skeleton keypoints using robust median depth like the hip, converting camera coordinates to trampoline frame following the established pipeline.
item_1:**Designing coordinate handling and body frame computation**

I'm deciding to store both camera and trampoline coordinates per keypoint, keeping existing pos3d as trampoline coords for height calculations and display, which matches metric consistency. I'm planning to compute per-pose orthonormal body axes in trampoline coordinates, derive relative rotation matrices using the bed frame rotation, and track cumulative per-axis rotations for flip/twist counts, while aligning tracking with stable hip detection for consistent person identity.
..