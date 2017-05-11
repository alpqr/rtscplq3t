PoC for dynamically created, hierarchical Qt3D scenes with keyframe animations.
(using the new AnimationClip API)

For example,

```
scene
  camera cam
  light light1
  model block block.obj
    model child_block block.obj
  model qtlogo qt_logo.obj

frames 20000
  0
    cam pos.z 3
    light1 pos.z 5
    qtlogo rot.x 60 trans.z -8 color green
    block trans.x -2.5 trans.z -5 rot.x 45 rot.y 45 rot.z 30 color red
    child_block trans.y 3 color blue
  2000
    qtlogo rot.z 90
    block trans.x 2
  5000
    qtlogo rot.y 90 scale.x 4 scale.y 4 scale.z 4
  6000
    block rot.z 60 trans.y 1
  10000
    qtlogo rot.y 180 rot.x 60 rot.z 0
    block color yellow
    child_block rot.z 0
  15000
    child_block rot.z 180
```

becomes a 20 sec keyframe-based animation.

![Image](https://raw.github.com/alpqr/rtscplq3t/master/rtscpl.png)
