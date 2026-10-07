# CMU paired handshake source motions

Downloaded on 2026-10-03 from the CMU Graphics Lab Motion Capture Database.

## Contents

- `18.asf`: subject A skeleton.
- `19.asf`: subject B skeleton.
- `18_01.amc` + `19_01.amc`: paired trial 1, walk and shake hands.
- `18_02.amc` + `19_02.amc`: paired trial 2, walk and shake hands.

These are original Acclaim ASF/AMC files, not FBX/BVH, Endfield AnimationClips, or a ready-to-load mod. Each AMC must be interpreted with its corresponding ASF. Preserve the paired trial timeline during conversion; verify alignment, scale, frame rate and retargeting before runtime playback.

## Sources

- https://mocap.cs.cmu.edu/
- https://mocap.cs.cmu.edu/search.php?maincat=1&subcat=1
- https://mocap.cs.cmu.edu/subjects/18/18.asf
- https://mocap.cs.cmu.edu/subjects/18/18_01.amc
- https://mocap.cs.cmu.edu/subjects/18/18_02.amc
- https://mocap.cs.cmu.edu/subjects/19/19.asf
- https://mocap.cs.cmu.edu/subjects/19/19_01.amc
- https://mocap.cs.cmu.edu/subjects/19/19_02.amc

## Usage terms and attribution

The CMU site states that the dataset is free for all uses and may be included in commercially sold products, but the motion data itself may not be resold directly, including converted versions. Consult the source site's terms before publishing a package.

Suggested acknowledgment from CMU:

The data used in this project was obtained from mocap.cs.cmu.edu.
The database was created with funding from NSF EIA-0196217.

CMU notes that hand/toe motion can be noisy, and finger/thumb motion is not captured. Hand contact and finger poses will need adjustment for the Endfield characters.
