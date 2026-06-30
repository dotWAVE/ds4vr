# ds4vr
Emulated VR controllers with a Dualshock4 or Dualsense gamepad.

## Disclaimer
All code in this repository was written by AI.

# Usage
A single Dualshock 4 or Dualsense gamepad will emulate a left and right Oculus Touch controller which can be posed independantly using the controller's gyroscope and thumbstick.
Hold right bumper to pose right hand.
Hold left bumper to pose left hand.
Bumpers can be held at the same time to move hands simultaneously.

## Pose Mode
- Move the the thumbstick forward or backward to move the arm closer or further. Left stick for left arm, right stick for right arm.
- Hold the thumbstick click to freeze the arm but continue rotating the hand. Left stick for left hand, right stick for right hand.
- Double click a stick to reset that hands rotation offset.
- Touble tab a bumper to reset that arm's pose and rotation offset.

## Basic controls (from ds4vr.ini)
```
[mapping.left]
; DS4 D-pad buttons → Touch left-hand components
dpad_up    = y
dpad_left  = x
dpad_down  = unbound
dpad_right = grip

[mapping.right]
; DS4 face buttons → Touch right-hand components
cross    = a
circle   = b
square   = grip
triangle = unbound
```


# Build and install.
Run scripts/build.ps1 to compile with cmake, install it with winget if you don't have it already.
scripts/install.ps1 to install the driver to SteamVR. Note: the driver will exist exclusively in the project folder, so be aware that if you delete or move the folder, SteamVR will likely no longer see it.

## Uninstall
scripts/uninstall.ps1 will remove the driver from SteamVR.

# Troubleshooting
## Spurious inputs while playing VRChat
VRChat process inputs from DualShock and DualSense controllers, even when playing in VR mode. This causes things like difficulty gripping objects and some DPAD inputs toggling the HUD and other such things. (See https://docs.vrchat.com/docs/gamepad)
The simplest workaround is to play without having the VRChat game window in focus.
