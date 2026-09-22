// Test-only OpenVR query fixture. Never packaged or installed as a runtime.
extern "C" __declspec(dllexport) bool VR_IsRuntimeInstalled(){return false;}
extern "C" __declspec(dllexport) bool VR_IsHmdPresent(){return false;}
