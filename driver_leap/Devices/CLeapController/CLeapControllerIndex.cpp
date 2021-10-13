#include "stdafx.h"

#include "Devices/CLeapController/CLeapControllerIndex.h"

#include "Devices/CLeapController/CControllerButton.h"
#include "Core/CDriverConfig.h"
#include "Utils/CGestureMatcher.h"
#include "Utils/Utils.h"

extern const glm::mat4 g_identityMatrix;
extern const vr::VRBoneTransform_t g_openHandGesture[];
extern const glm::vec4 g_zeroPoint;

enum HandFinger : size_t
{
    HF_Thumb = 0U,
    HF_Index,
    HF_Middle,
    HF_Ring,
    HF_Pinky,

    HF_Count
};

enum IndexButton : size_t
{
    IB_SystemClick = 0U,
    IB_SystemTouch,
    IB_TriggerClick,
    IB_TriggerValue,
    IB_TrackpadX,
    IB_TrackpadY,
    IB_TrackpadTouch,
    IB_TrackpadForce,
    IB_GripTouch,
    IB_GripForce,
    IB_GripValue,
    IB_ThumbstickClick,
    IB_ThumbstickTouch,
    IB_ThumbstickX,
    IB_ThumbstickY,
    IB_AClick,
    IB_ATouch,
    IB_BClick,
    IB_BTouch,
    IB_FingerIndex,
    IB_FingerMiddle,
    IB_FingerRing,
    IB_FingerPinky,

    IB_Count
};

CLeapControllerIndex::CLeapControllerIndex(unsigned char f_hand)
{
    m_hand = (f_hand % CH_Count);
    m_type = CT_IndexKnuckle;
    m_serialNumber.assign((m_hand == CH_Left) ? "LHR-E217CD00" : "LHR-E217CD01");

    for(size_t i = 0U; i < HSB_Count; i++) m_boneTransform[i] = g_openHandGesture[i];
    m_skeletonHandle = vr::k_ulInvalidInputComponentHandle;

    if(m_hand == CH_Right)
    {
        // Transformation inversion along 0YZ plane
        for(size_t i = 1U; i < HSB_Count; i++)
        {
            m_boneTransform[i].position.v[0] *= -1.f;

            switch(i)
            {
                case HSB_Wrist:
                {
                    m_boneTransform[i].orientation.y *= -1.f;
                    m_boneTransform[i].orientation.z *= -1.f;
                } break;

                case HSB_Thumb0:
                case HSB_IndexFinger0:
                case HSB_MiddleFinger0:
                case HSB_RingFinger0:
                case HSB_PinkyFinger0:
                {
                    m_boneTransform[i].orientation.z *= -1.f;
                    std::swap(m_boneTransform[i].orientation.x, m_boneTransform[i].orientation.w);
                    m_boneTransform[i].orientation.w *= -1.f;
                    std::swap(m_boneTransform[i].orientation.y, m_boneTransform[i].orientation.z);
                } break;
            }
        }
    }

}

CLeapControllerIndex::~CLeapControllerIndex()
{
}

void CLeapControllerIndex::ChangeBoneOrientation(glm::quat &f_rot)
{
    std::swap(f_rot.x, f_rot.z);
    f_rot.z *= -1.f;
    if(m_hand == CH_Left)
    {
        f_rot.x *= -1.f;
        f_rot.y *= -1.f;
    }
}

void CLeapControllerIndex::ChangeAuxTransformation(glm::vec3 &f_pos, glm::quat &f_rot)
{
    f_pos.y *= -1.f;
    f_pos.z *= -1.f;

    std::swap(f_rot.x, f_rot.w);
    f_rot.w *= -1.f;
    std::swap(f_rot.y, f_rot.z);
    f_rot.y *= -1.f;
}

size_t CLeapControllerIndex::GetFingerBoneIndex(size_t f_id)
{
    size_t l_result = 0U;
    switch(f_id)
    {
        case HF_Thumb:
            l_result = HSB_Thumb0;
            break;
        case HF_Index:
            l_result = HSB_IndexFinger0;
            break;
        case HF_Middle:
            l_result = HSB_MiddleFinger0;
            break;
        case HF_Ring:
            l_result = HSB_RingFinger0;
            break;
        case HF_Pinky:
            l_result = HSB_PinkyFinger0;
            break;
    }
    return l_result;
}

void CLeapControllerIndex::ActivateInternal()
{
    // Properties
    vr::VRProperties()->SetStringProperty(m_propertyContainer, vr::Prop_TrackingSystemName_String, "lighthouse");
    vr::VRProperties()->SetStringProperty(m_propertyContainer, vr::Prop_SerialNumber_String, m_serialNumber.c_str());
    vr::VRProperties()->SetBoolProperty(m_propertyContainer, vr::Prop_WillDriftInYaw_Bool, false);
    vr::VRProperties()->SetBoolProperty(m_propertyContainer, vr::Prop_DeviceIsWireless_Bool, true);
    vr::VRProperties()->SetBoolProperty(m_propertyContainer, vr::Prop_DeviceIsCharging_Bool, false);
    vr::VRProperties()->SetFloatProperty(m_propertyContainer, vr::Prop_DeviceBatteryPercentage_Float, 1.f); // Always charged

    vr::HmdMatrix34_t l_matrix = { -1.f, 0.f, 0.f, 0.f, 0.f, 0.f, -1.f, 0.f, 0.f, -1.f, 0.f, 0.f };
    vr::VRProperties()->SetProperty(m_propertyContainer, vr::Prop_StatusDisplayTransform_Matrix34, &l_matrix, sizeof(vr::HmdMatrix34_t), vr::k_unHmdMatrix34PropertyTag);

    vr::VRProperties()->SetBoolProperty(m_propertyContainer, vr::Prop_Firmware_UpdateAvailable_Bool, false);
    vr::VRProperties()->SetBoolProperty(m_propertyContainer, vr::Prop_Firmware_ManualUpdate_Bool, false);
    vr::VRProperties()->SetStringProperty(m_propertyContainer, vr::Prop_Firmware_ManualUpdateURL_String, "https://developer.valvesoftware.com/wiki/SteamVR/HowTo_Update_Firmware");
    vr::VRProperties()->SetBoolProperty(m_propertyContainer, vr::Prop_DeviceProvidesBatteryStatus_Bool, true);
    vr::VRProperties()->SetBoolProperty(m_propertyContainer, vr::Prop_DeviceCanPowerOff_Bool, true);
    vr::VRProperties()->SetInt32Property(m_propertyContainer, vr::Prop_DeviceClass_Int32, vr::TrackedDeviceClass_Controller);
    vr::VRProperties()->SetBoolProperty(m_propertyContainer, vr::Prop_Firmware_ForceUpdateRequired_Bool, false);
    vr::VRProperties()->SetBoolProperty(m_propertyContainer, vr::Prop_Identifiable_Bool, true);
    vr::VRProperties()->SetBoolProperty(m_propertyContainer, vr::Prop_Firmware_RemindUpdate_Bool, false);
    vr::VRProperties()->SetInt32Property(m_propertyContainer, vr::Prop_Axis0Type_Int32, vr::k_eControllerAxis_TrackPad);
    vr::VRProperties()->SetInt32Property(m_propertyContainer, vr::Prop_Axis1Type_Int32, vr::k_eControllerAxis_Trigger);
    vr::VRProperties()->SetInt32Property(m_propertyContainer, vr::Prop_ControllerRoleHint_Int32, (m_hand == CH_Left) ? vr::TrackedControllerRole_LeftHand : vr::TrackedControllerRole_RightHand);
    vr::VRProperties()->SetBoolProperty(m_propertyContainer, vr::Prop_HasDisplayComponent_Bool, false);
    vr::VRProperties()->SetBoolProperty(m_propertyContainer, vr::Prop_HasCameraComponent_Bool, false);
    vr::VRProperties()->SetBoolProperty(m_propertyContainer, vr::Prop_HasDriverDirectModeComponent_Bool, false);
    vr::VRProperties()->SetBoolProperty(m_propertyContainer, vr::Prop_HasVirtualDisplayComponent_Bool, false);
    vr::VRProperties()->SetInt32Property(m_propertyContainer, vr::Prop_ControllerHandSelectionPriority_Int32, -10);
    vr::VRProperties()->SetStringProperty(m_propertyContainer, vr::Prop_ModelNumber_String, (m_hand == CH_Left) ? "Leap Left" : "Leap Right");
    //vr::VRProperties()->SetStringProperty(m_propertyContainer, vr::Prop_RenderModelName_String, (m_hand == CH_Left) ? "{leap}valve_controller_knu_1_0_left" : "{leap}valve_controller_knu_1_0_right");
    vr::VRProperties()->SetStringProperty(m_propertyContainer, vr::Prop_ManufacturerName_String, "Valve");
    vr::VRProperties()->SetStringProperty(m_propertyContainer, vr::Prop_TrackingFirmwareVersion_String, "1562916277 watchman@ValveBuilder02 2019-07-12 FPGA 538(2.26/10/2) BL 0 VRC 1562916277 Radio 1562882729");
    vr::VRProperties()->SetStringProperty(m_propertyContainer, vr::Prop_HardwareRevision_String, "product 17 rev 14.1.9 lot 2019/4/20 0");
    vr::VRProperties()->SetStringProperty(m_propertyContainer, vr::Prop_ConnectedWirelessDongle_String, "C2F75F5986-DIY"); // Changed
    vr::VRProperties()->SetUint64Property(m_propertyContainer, vr::Prop_HardwareRevision_Uint64, 286130441U);
    vr::VRProperties()->SetUint64Property(m_propertyContainer, vr::Prop_FirmwareVersion_Uint64, 1562916277U);
    vr::VRProperties()->SetUint64Property(m_propertyContainer, vr::Prop_FPGAVersion_Uint64, 538U);
    vr::VRProperties()->SetUint64Property(m_propertyContainer, vr::Prop_VRCVersion_Uint64, 1562916277U);
    vr::VRProperties()->SetUint64Property(m_propertyContainer, vr::Prop_RadioVersion_Uint64, 1562882729U);
    vr::VRProperties()->SetUint64Property(m_propertyContainer, vr::Prop_DongleVersion_Uint64, 1558748372U);
    vr::VRProperties()->SetStringProperty(m_propertyContainer, vr::Prop_Firmware_ProgrammingTarget_String, (m_hand == CH_Left) ? "LHR-E217CD00" : "LHR-E217CD01"); // Changed
    vr::VRProperties()->SetStringProperty(m_propertyContainer, vr::Prop_ResourceRoot_String, "leap");
    vr::VRProperties()->SetStringProperty(m_propertyContainer, vr::Prop_RegisteredDeviceType_String, (m_hand == CH_Left) ? "valve/index_controllerLHR-E217CD00" : "valve/index_controllerLHR-E217CD01"); // Changed
    vr::VRProperties()->SetStringProperty(m_propertyContainer, vr::Prop_InputProfilePath_String, "{leap}/input/index_controller_profile.json");
    vr::VRProperties()->SetInt32Property(m_propertyContainer, vr::Prop_Axis2Type_Int32, vr::k_eControllerAxis_Trigger);
    vr::VRProperties()->SetStringProperty(m_propertyContainer, vr::Prop_NamedIconPathDeviceOff_String, (m_hand == CH_Left) ? "{leap}/icons/left_controller_status_off.png" : "{leap}/icons/right_controller_status_off.png");
    vr::VRProperties()->SetStringProperty(m_propertyContainer, vr::Prop_NamedIconPathDeviceSearching_String, (m_hand == CH_Left) ? "{leap}/icons/left_controller_status_searching.gif" : "{leap}/icons/right_controller_status_searching.gif");
    vr::VRProperties()->SetStringProperty(m_propertyContainer, vr::Prop_NamedIconPathDeviceSearchingAlert_String, (m_hand == CH_Left) ? "{leap}/icons/left_controller_status_searching_alert.gif" : "{leap}/icons//right_controller_status_searching_alert.gif");
    vr::VRProperties()->SetStringProperty(m_propertyContainer, vr::Prop_NamedIconPathDeviceReady_String, (m_hand == CH_Left) ? "{leap}/icons/left_controller_status_ready.png" : "{leap}/icons//right_controller_status_ready.png");
    vr::VRProperties()->SetStringProperty(m_propertyContainer, vr::Prop_NamedIconPathDeviceReadyAlert_String, (m_hand == CH_Left) ? "{leap}/icons/left_controller_status_ready_alert.png" : "{leap}/icons//right_controller_status_ready_alert.png");
    vr::VRProperties()->SetStringProperty(m_propertyContainer, vr::Prop_NamedIconPathDeviceNotReady_String, (m_hand == CH_Left) ? "{leap}/icons/left_controller_status_error.png" : "{leap}/icons//right_controller_status_error.png");
    vr::VRProperties()->SetStringProperty(m_propertyContainer, vr::Prop_NamedIconPathDeviceStandby_String, (m_hand == CH_Left) ? "{leap}/icons/left_controller_status_off.png" : "{leap}/icons//right_controller_status_off.png");
    vr::VRProperties()->SetStringProperty(m_propertyContainer, vr::Prop_NamedIconPathDeviceAlertLow_String, (m_hand == CH_Left) ? "{leap}/icons/left_controller_status_ready_low.png" : "{leap}/icons//right_controller_status_ready_low.png");
    vr::VRProperties()->SetStringProperty(m_propertyContainer, vr::Prop_ControllerType_String, "knuckles");

    // Input
    if(m_buttons.empty())
    {
        for(size_t i = 0U; i < IB_Count; i++) m_buttons.push_back(new CControllerButton());
    }

    vr::VRDriverInput()->CreateBooleanComponent(m_propertyContainer, "/input/system/click", &m_buttons[IB_SystemClick]->GetHandleRef());
    m_buttons[IB_SystemClick]->SetInputType(CControllerButton::IT_Boolean);

    vr::VRDriverInput()->CreateBooleanComponent(m_propertyContainer, "/input/system/touch", &m_buttons[IB_SystemTouch]->GetHandleRef());
    m_buttons[IB_SystemTouch]->SetInputType(CControllerButton::IT_Boolean);

    vr::VRDriverInput()->CreateBooleanComponent(m_propertyContainer, "/input/trigger/click", &m_buttons[IB_TriggerClick]->GetHandleRef());
    m_buttons[IB_TriggerClick]->SetInputType(CControllerButton::IT_Boolean);

    vr::VRDriverInput()->CreateScalarComponent(m_propertyContainer, "/input/trigger/value", &m_buttons[IB_TriggerValue]->GetHandleRef(), vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedOneSided);
    m_buttons[IB_TriggerValue]->SetInputType(CControllerButton::IT_Float);

    vr::VRDriverInput()->CreateScalarComponent(m_propertyContainer, "/input/trackpad/x", &m_buttons[IB_TrackpadX]->GetHandleRef(), vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedTwoSided);
    m_buttons[IB_TrackpadX]->SetInputType(CControllerButton::IT_Float);

    vr::VRDriverInput()->CreateScalarComponent(m_propertyContainer, "/input/trackpad/y", &m_buttons[IB_TrackpadY]->GetHandleRef(), vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedTwoSided);
    m_buttons[IB_TrackpadY]->SetInputType(CControllerButton::IT_Float);

    vr::VRDriverInput()->CreateBooleanComponent(m_propertyContainer, "/input/trackpad/touch", &m_buttons[IB_TrackpadTouch]->GetHandleRef());
    m_buttons[IB_TrackpadTouch]->SetInputType(CControllerButton::IT_Boolean);

    vr::VRDriverInput()->CreateScalarComponent(m_propertyContainer, "/input/trackpad/force", &m_buttons[IB_TrackpadForce]->GetHandleRef(), vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedOneSided);
    m_buttons[IB_TrackpadForce]->SetInputType(CControllerButton::IT_Float);

    vr::VRDriverInput()->CreateBooleanComponent(m_propertyContainer, "/input/grip/touch", &m_buttons[IB_GripTouch]->GetHandleRef());
    m_buttons[IB_GripTouch]->SetInputType(CControllerButton::IT_Boolean);

    vr::VRDriverInput()->CreateScalarComponent(m_propertyContainer, "/input/grip/force", &m_buttons[IB_GripForce]->GetHandleRef(), vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedOneSided);
    m_buttons[IB_GripForce]->SetInputType(CControllerButton::IT_Float);

    vr::VRDriverInput()->CreateScalarComponent(m_propertyContainer, "/input/grip/value", &m_buttons[IB_GripValue]->GetHandleRef(), vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedOneSided);
    m_buttons[IB_GripValue]->SetInputType(CControllerButton::IT_Float);

    vr::VRDriverInput()->CreateBooleanComponent(m_propertyContainer, "/input/thumbstick/click", &m_buttons[IB_ThumbstickClick]->GetHandleRef());
    m_buttons[IB_ThumbstickClick]->SetInputType(CControllerButton::IT_Boolean);

    vr::VRDriverInput()->CreateBooleanComponent(m_propertyContainer, "/input/thumbstick/touch", &m_buttons[IB_ThumbstickTouch]->GetHandleRef());
    m_buttons[IB_ThumbstickTouch]->SetInputType(CControllerButton::IT_Boolean);

    vr::VRDriverInput()->CreateScalarComponent(m_propertyContainer, "/input/thumbstick/x", &m_buttons[IB_ThumbstickX]->GetHandleRef(), vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedTwoSided);
    m_buttons[IB_ThumbstickX]->SetInputType(CControllerButton::IT_Float);

    vr::VRDriverInput()->CreateScalarComponent(m_propertyContainer, "/input/thumbstick/y", &m_buttons[IB_ThumbstickY]->GetHandleRef(), vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedTwoSided);
    m_buttons[IB_ThumbstickY]->SetInputType(CControllerButton::IT_Float);

    vr::VRDriverInput()->CreateBooleanComponent(m_propertyContainer, "/input/a/click", &m_buttons[IB_AClick]->GetHandleRef());
    m_buttons[IB_AClick]->SetInputType(CControllerButton::IT_Boolean);

    vr::VRDriverInput()->CreateBooleanComponent(m_propertyContainer, "/input/a/touch", &m_buttons[IB_ATouch]->GetHandleRef());
    m_buttons[IB_ATouch]->SetInputType(CControllerButton::IT_Boolean);

    vr::VRDriverInput()->CreateBooleanComponent(m_propertyContainer, "/input/b/click", &m_buttons[IB_BClick]->GetHandleRef());
    m_buttons[IB_BClick]->SetInputType(CControllerButton::IT_Boolean);

    vr::VRDriverInput()->CreateBooleanComponent(m_propertyContainer, "/input/b/touch", &m_buttons[IB_BTouch]->GetHandleRef());
    m_buttons[IB_BTouch]->SetInputType(CControllerButton::IT_Boolean);

    vr::VRDriverInput()->CreateScalarComponent(m_propertyContainer, "/input/finger/index", &m_buttons[IB_FingerIndex]->GetHandleRef(), vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedOneSided);
    m_buttons[IB_FingerIndex]->SetInputType(CControllerButton::IT_Float);

    vr::VRDriverInput()->CreateScalarComponent(m_propertyContainer, "/input/finger/middle", &m_buttons[IB_FingerMiddle]->GetHandleRef(), vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedOneSided);
    m_buttons[IB_FingerMiddle]->SetInputType(CControllerButton::IT_Float);

    vr::VRDriverInput()->CreateScalarComponent(m_propertyContainer, "/input/finger/ring", &m_buttons[IB_FingerRing]->GetHandleRef(), vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedOneSided);
    m_buttons[IB_FingerRing]->SetInputType(CControllerButton::IT_Float);

    vr::VRDriverInput()->CreateScalarComponent(m_propertyContainer, "/input/finger/pinky", &m_buttons[IB_FingerPinky]->GetHandleRef(), vr::VRScalarType_Absolute, vr::VRScalarUnits_NormalizedOneSided);
    m_buttons[IB_FingerPinky]->SetInputType(CControllerButton::IT_Float);

    const vr::EVRSkeletalTrackingLevel l_trackingLevel = ((CDriverConfig::GetTrackingLevel() == CDriverConfig::TL_Partial) ? vr::VRSkeletalTracking_Partial : vr::VRSkeletalTracking_Full);
    switch(m_hand)
    {
        case CH_Left:
            vr::VRDriverInput()->CreateSkeletonComponent(m_propertyContainer, "/input/skeleton/left", "/skeleton/hand/left", "/pose/raw", l_trackingLevel, nullptr, 0U, &m_skeletonHandle);
            break;
        case CH_Right:
            vr::VRDriverInput()->CreateSkeletonComponent(m_propertyContainer, "/input/skeleton/right", "/skeleton/hand/right", "/pose/raw", l_trackingLevel, nullptr, 0U, &m_skeletonHandle);
            break;
    }

    vr::VRDriverInput()->CreateHapticComponent(m_propertyContainer, "/output/haptic", &m_haptic);
}

void CLeapControllerIndex::UpdateGestures(const LEAP_HAND *f_hand, const LEAP_HAND *f_oppHand)
{
    if(f_hand)
    {
        std::vector<float> l_gestures;
        CGestureMatcher::GetGestures(f_hand, l_gestures, f_oppHand);

        m_buttons[IB_TriggerValue]->SetValue(l_gestures[CGestureMatcher::HG_Trigger]);
        m_buttons[IB_TriggerClick]->SetState(l_gestures[CGestureMatcher::HG_Trigger] >= 0.75f);

        m_buttons[IB_GripValue]->SetValue(l_gestures[CGestureMatcher::HG_Grab]);
        m_buttons[IB_GripTouch]->SetState(l_gestures[CGestureMatcher::HG_Grab] >= 0.25f);
        m_buttons[IB_GripForce]->SetValue((l_gestures[CGestureMatcher::HG_Grab] >= 0.75f) ? (l_gestures[CGestureMatcher::HG_Grab] - 0.75f) * 4.f : 0.f);

        m_buttons[IB_TrackpadTouch]->SetState(l_gestures[CGestureMatcher::HG_ThumbPress] >= 0.5f);
        m_buttons[IB_TrackpadForce]->SetState((l_gestures[CGestureMatcher::HG_ThumbPress] >= 0.5f) ? (l_gestures[CGestureMatcher::HG_ThumbPress] - 0.5f) *2.f : 0.f);
        if(l_gestures[CGestureMatcher::HG_ThumbPress] >= 0.5f)
        {
            m_buttons[IB_TrackpadX]->SetValue(l_gestures[CGestureMatcher::HG_PalmPointX]);
            m_buttons[IB_TrackpadY]->SetValue(l_gestures[CGestureMatcher::HG_PalmPointY]);
        }
        else
        {
            m_buttons[IB_TrackpadX]->SetValue(0.f);
            m_buttons[IB_TrackpadY]->SetValue(0.f);
        }

        m_buttons[IB_SystemTouch]->SetState(l_gestures[CGestureMatcher::HG_OpisthenarTouch] >= 0.5f);
        m_buttons[IB_SystemClick]->SetState(l_gestures[CGestureMatcher::HG_OpisthenarTouch] >= 0.75f);

        m_buttons[IB_BTouch]->SetState(l_gestures[CGestureMatcher::HG_PalmTouch] >= 0.5f);
        m_buttons[IB_BClick]->SetState(l_gestures[CGestureMatcher::HG_PalmTouch] >= 0.75f);

        m_buttons[IB_ATouch]->SetState(l_gestures[CGestureMatcher::HG_MiddleCrossTouch] >= 0.5f);
        m_buttons[IB_AClick]->SetState(l_gestures[CGestureMatcher::HG_MiddleCrossTouch] >= 0.75f);

        m_buttons[IB_ThumbstickTouch]->SetState(l_gestures[CGestureMatcher::HG_ThumbCrossTouch] >= 0.5f);
        m_buttons[IB_ThumbstickClick]->SetState(l_gestures[CGestureMatcher::HG_ThumbCrossTouch] >= 0.75f);

        m_buttons[IB_FingerIndex]->SetValue(l_gestures[CGestureMatcher::HG_IndexBend]);
        m_buttons[IB_FingerMiddle]->SetValue(l_gestures[CGestureMatcher::HG_MiddleBend]);
        m_buttons[IB_FingerRing]->SetValue(l_gestures[CGestureMatcher::HG_RingBend]);
        m_buttons[IB_FingerPinky]->SetValue(l_gestures[CGestureMatcher::HG_PinkyBend]);

        const glm::quat rotateHalfPiZ(0.70106769f, 0.f, 0.f, 0.70106769f);
        const glm::quat rotateHalfPiX(0.70106769f, 0.70106769f, 0.f, 0.f);
        const glm::quat rotateHalfPiY(0.70106769f, 0.f, 0.70106769f, 0.f);
        const glm::quat rotateQuarterPiY(0.9238795f, 0.f, -0.3826834f, 0.f);

        LEAP_QUATERNION palmOrient_leap = f_hand->palm.orientation;
        glm::quat palmRotation;
        ConvertQuaternion(palmOrient_leap, palmRotation);
        const glm::quat palmRotationInv = glm::inverse(palmRotation);

        // Reset all points for good measure
        for (size_t i = 0; i < 31; i++)
        {
            m_boneTransform[i].position = { 0.0, 0.0, 0.0, 1.0 };
            m_boneTransform[i].orientation = { 1.0, 0.0, 0.0, 0.0 };
        }

        /*m_boneTransform[HSB_Wrist].position.v[0] = 0.0;
        m_boneTransform[HSB_Wrist].position.v[1] = 0.0;
        m_boneTransform[HSB_Wrist].position.v[2] = -0.03;
        m_boneTransform[HSB_Wrist].position.v[3] = 1.0;*/

        glm::quat wrist_rot = rotateHalfPiZ * rotateHalfPiZ * rotateHalfPiZ * rotateHalfPiX * rotateHalfPiX;
        if (m_hand == CH_Left)
            wrist_rot = wrist_rot * rotateHalfPiZ * rotateHalfPiZ;
        ConvertQuaternion(wrist_rot, m_boneTransform[HSB_Wrist].orientation);

        LEAP_VECTOR leap_ring0 = f_hand->digits[HF_Ring].bones[0].prev_joint;
        LEAP_VECTOR leap_ring1 = f_hand->digits[HF_Ring].bones[1].prev_joint;
        LEAP_VECTOR leap_middle0 = f_hand->digits[HF_Middle].bones[0].prev_joint;
        LEAP_VECTOR leap_middle1 = f_hand->digits[HF_Middle].bones[1].prev_joint;
        LEAP_VECTOR leap_wrist = f_hand->arm.next_joint;
        glm::vec3 unrotateXBasis, unrotateYBasis, unrotateZBasis;
        glm::vec3 _ring0(leap_ring0.x, -leap_ring0.y, leap_ring0.z);
        glm::vec3 _ring1(leap_ring1.x, -leap_ring1.y, leap_ring1.z);
        glm::vec3 _middle0(leap_middle0.x, -leap_middle0.y, leap_middle0.z);
        glm::vec3 _middle1(leap_middle1.x, -leap_middle1.y, leap_middle1.z);
        glm::vec3 _wrist(leap_wrist.x, -leap_wrist.y, leap_wrist.z);
        glm::mat3 unrotateTransform, unrotateTransformInv;

        glm::vec3 _zbasis_midpoint = (_middle1 + _ring1);
        _zbasis_midpoint /= 2;

        unrotateYBasis = _ring0 - _middle0;
        unrotateYBasis /= glm::length(unrotateYBasis);
        //unrotateZBasis = _middle0 - _middle1;
        unrotateZBasis = _wrist - _zbasis_midpoint;
        
        unrotateXBasis = glm::cross(unrotateZBasis, unrotateYBasis);
        unrotateXBasis /= glm::length(unrotateXBasis);
        unrotateZBasis = glm::cross(unrotateXBasis, unrotateYBasis);

        unrotateTransform[0] = unrotateXBasis;
        unrotateTransform[1] = unrotateYBasis;
        unrotateTransform[2] = -unrotateZBasis;
        unrotateTransformInv = glm::inverse(unrotateTransform);

        glm::vec3 unrotateTranslate = _wrist * unrotateTransformInv;

        for(size_t i = 0U; i < 5U; i++)
        {
            const LEAP_DIGIT &l_finger = f_hand->digits[i];
            size_t l_transformIndex = GetFingerBoneIndex(i);
            

            LEAP_QUATERNION l_leapRotation = f_hand->palm.orientation;

            const glm::quat reverseRotation(0.f, 0.f, 0.70106769f, -0.70106769f);
            glm::quat l_segmentRotation (l_leapRotation.w, l_leapRotation.x, l_leapRotation.y, l_leapRotation.z);
            
            LEAP_VECTOR l_boneRoot = f_hand->arm.next_joint;
            glm::vec3 l_position(l_boneRoot.x * -0.001, l_boneRoot.y * -0.001, -l_boneRoot.z * -0.001);
            glm::quat l_rotation;
            ConvertQuaternion(l_leapRotation, l_rotation);
            l_rotation *= wrist_rot;
            l_segmentRotation *= wrist_rot * rotateHalfPiX * rotateHalfPiX * rotateHalfPiX;

            glm::vec3 l_position_prev = l_position;
            glm::vec3 l_root = l_position;
            glm::quat l_rootRot = l_rotation;
            glm::quat l_rootRotInv = glm::inverse(l_rootRot);

            for(size_t j = 0U; j < 4U; j++)
            {
                if (j == 0 && GetFingerBoneIndex(i) == HSB_Thumb0) continue;
                glm::quat l_prevSegmentRotation = l_segmentRotation;
                glm::quat l_prevSegmentRotationInv = glm::inverse(l_segmentRotation);
                l_leapRotation = l_finger.bones[j].rotation;
                ConvertQuaternion(l_leapRotation, l_segmentRotation);

                glm::quat l_segmentResult = l_prevSegmentRotationInv*l_segmentRotation;

                if (!(j == 0 || l_transformIndex == HSB_Thumb0)) //
                    ChangeBoneOrientation(l_segmentResult);

                if (l_transformIndex == HSB_Thumb0)
                {

                    if (m_hand == CH_Left) {
                        l_segmentResult.z *= -1;
                        l_segmentResult.x *= -1;
                        std::swap(l_segmentResult.x, l_segmentResult.z);

                        l_segmentResult *= rotateHalfPiY * rotateHalfPiY * rotateHalfPiZ * rotateHalfPiX;
                        l_segmentResult *= rotateHalfPiX * rotateHalfPiY;// *rotateHalfPiX;
                        //l_segmentResult.x *= -1;
                        l_segmentResult.z *= -1;
                        //l_segmentResult.y *= -1;
                    }
                    else
                    {
                        ChangeBoneOrientation(l_segmentResult);
                        
                        // Helpers for figuring out rotation issues
                        static int throttle = 0;
                        static int test = 2;
                        static int test2 = 2;
                        static int test3 = 1;

                        l_segmentResult *= rotateHalfPiX * rotateHalfPiX * rotateHalfPiY * rotateHalfPiY * rotateHalfPiZ * rotateQuarterPiY;

                        for (int k = 0; k < test; k++)
                        {
                            //l_segmentResult *= rotateHalfPiY;
                        }
                        for (int k = 0; k < test2; k++)
                        {
                            //l_segmentResult *= rotateHalfPiY;
                        }
                        for (int k = 0; k < test3; k++)
                        {
                            //l_segmentResult *= rotateHalfPiZ;
                        }

                        throttle++;
                        //if (throttle == 90*5)
                            test++;
                        if (test == 4)
                            test2++;
                        if (test2 == 4)
                            test3++;
                        test3 = test3 % 4;
                        test = test % 4;
                        test2 = test2 % 4;
                        throttle = throttle % (90 * 5);
                        //l_segmentResult *= rotateHalfPiX * rotateHalfPiZ * rotateHalfPiZ * rotateHalfPiZ;// *rotateHalfPiX;
                        //l_segmentResult.x *= -1;
                        l_segmentResult.z *= -1;
                        //l_segmentResult.y *= -1;
                    }
                }

                if (m_hand == CH_Left && (j == 0 || l_transformIndex == HSB_Thumb0))
                {
                    l_segmentResult *= rotateHalfPiZ * rotateHalfPiZ * rotateHalfPiY * rotateHalfPiY;
                }

                // TODO the right hand thumb is still weird
                if (m_hand == CH_Right && GetFingerBoneIndex(i) == HSB_Thumb0 && l_transformIndex != HSB_Thumb0)
                {
                    //l_segmentResult.z *= -1;
                    //l_segmentResult.x *= -1;
                    //l_segmentResult.z *= -1;
                    l_segmentResult.y *= -1;
                    //std::swap(l_segmentResult.x, l_segmentResult.z);
                }

                // TODO I don't really use all of this

                l_position = glm::vec3(l_finger.bones[j].prev_joint.x * -0.001, l_finger.bones[j].prev_joint.y * -0.001, -l_finger.bones[j].prev_joint.z * -0.001);

                glm::mat4 joint_transform = glm::translate(g_identityMatrix, l_position);
                
                joint_transform *= glm::translate(g_identityMatrix, -l_position_prev);

                l_position_prev = l_position;
                l_position = joint_transform * g_zeroPoint;

                l_boneRoot = l_finger.bones[j].prev_joint;

                const double l_knuckleWidth = (f_hand->palm.width * 0.001) / 5.0;

// Fixed index 0, not using the actual data at all 
#if 1
                // How much to shift each finger inwards
                const double rootDepthShift[5] = { 0.02, 0.005, 0.01, 0.012, 0.007 };
                if (l_transformIndex == HSB_Thumb0)
                {
                    m_boneTransform[l_transformIndex].position.v[1] = -(l_knuckleWidth * 2.5) + ((4) * l_knuckleWidth) + (l_knuckleWidth * 0.5);//l_position.x;
                    m_boneTransform[l_transformIndex].position.v[0] = (m_hand == CH_Left) ? -0.025 : 0.025;//l_position.y;
                    m_boneTransform[l_transformIndex].position.v[2] = rootDepthShift[i];//l_position.z;
                    m_boneTransform[l_transformIndex].position.v[3] = 1.0;

                    ConvertQuaternion(l_segmentResult, m_boneTransform[l_transformIndex].orientation);
                }
                else if (j == 0 || l_transformIndex == HSB_Thumb0)
                {
                    m_boneTransform[l_transformIndex].position.v[1] = -(l_knuckleWidth * 2.5) + ((4-(i-1)) * l_knuckleWidth);//l_position.x;
                    m_boneTransform[l_transformIndex].position.v[0] = 0.0;//l_position.y;
                    m_boneTransform[l_transformIndex].position.v[2] = rootDepthShift[i];//l_position.z;
                    m_boneTransform[l_transformIndex].position.v[3] = 1.0;

                    ConvertQuaternion(l_segmentResult, m_boneTransform[l_transformIndex].orientation);
                }
#endif

// Dynamic index 0, attempt to use LeapC's bone[0] joints
#if 0
                if (j == 0 || l_transformIndex == HSB_Thumb0)
                {
                    l_position = glm::vec3(l_finger.bones[j].prev_joint.x, -l_finger.bones[j].prev_joint.y, l_finger.bones[j].prev_joint.z);
                    l_position = l_position * unrotateTransformInv;
                    l_position -= unrotateTranslate;
                    l_position *= -0.001;
                    //l_position.z *= -1.0;

                    m_boneTransform[l_transformIndex].position.v[0] = -l_position.x;
                    m_boneTransform[l_transformIndex].position.v[1] = l_position.y;//l_position.y;
                    m_boneTransform[l_transformIndex].position.v[2] = l_position.z;
                    m_boneTransform[l_transformIndex].position.v[3] = 1.0;

                    ConvertQuaternion(l_segmentResult, m_boneTransform[l_transformIndex].orientation);
                }
#endif
                else
                {
                    m_boneTransform[l_transformIndex].position.v[2] = 0.0;
                    m_boneTransform[l_transformIndex].position.v[1] = 0.0;
                    m_boneTransform[l_transformIndex].position.v[0] = (m_hand == CH_Left) ? glm::length(l_position) : -glm::length(l_position);
                    m_boneTransform[l_transformIndex].position.v[3] = 1.0;

                    ConvertQuaternion(l_segmentResult, m_boneTransform[l_transformIndex].orientation);
                }

                
                l_transformIndex++;
            }

            l_position = glm::vec3(l_finger.bones[3].next_joint.x * -0.001, l_finger.bones[3].next_joint.y * -0.001, -l_finger.bones[3].next_joint.z * -0.001);

            m_boneTransform[l_transformIndex].position.v[2] = 0.0;//l_position.x;
            m_boneTransform[l_transformIndex].position.v[1] = 0.0;//l_position.y;
            m_boneTransform[l_transformIndex].position.v[0] = (m_hand == CH_Left)  ? glm::length(l_position - l_position_prev) : -glm::length(l_position - l_position_prev);//l_position.z;
            m_boneTransform[l_transformIndex].position.v[3] = 1.0;

        }

#if 1
        // Update aux bones
        glm::vec3 l_position;
        glm::quat l_rotation;
        ConvertVector3(m_boneTransform[HSB_Wrist].position, l_position);
        ConvertQuaternion(m_boneTransform[HSB_Wrist].orientation, l_rotation);
        const glm::mat4 l_wristMat = glm::translate(g_identityMatrix, l_position) * glm::mat4_cast(l_rotation);

        for(size_t i = HF_Thumb; i < HF_Count; i++)
        {
            glm::mat4 l_chainMat(l_wristMat);
            const size_t l_chainIndex = GetFingerBoneIndex(i);
            for(size_t j = 0U; j < ((i == HF_Thumb) ? 3U : 4U); j++)
            {
                ConvertVector3(m_boneTransform[l_chainIndex + j].position, l_position);
                ConvertQuaternion(m_boneTransform[l_chainIndex + j].orientation, l_rotation);
                l_chainMat = l_chainMat*(glm::translate(g_identityMatrix, l_position)*glm::mat4_cast(l_rotation));
            }
            l_position = l_chainMat*g_zeroPoint;
            l_rotation = glm::quat_cast(l_chainMat);
            //if(m_hand == CH_Left) ChangeAuxTransformation(l_position, l_rotation);
            if (m_hand == CH_Left)
            {
                l_rotation *= rotateHalfPiZ * rotateHalfPiZ * rotateHalfPiY * rotateHalfPiY;
            }

            ConvertVector3(l_position, m_boneTransform[HSB_Aux_Thumb + i].position);
            ConvertQuaternion(l_rotation, m_boneTransform[HSB_Aux_Thumb + i].orientation);
        }
#endif
    }
}

void CLeapControllerIndex::UpdateInputInternal()
{
    vr::VRDriverInput()->UpdateSkeletonComponent(m_skeletonHandle, vr::VRSkeletalMotionRange_WithController, m_boneTransform, HSB_Count);
    vr::VRDriverInput()->UpdateSkeletonComponent(m_skeletonHandle, vr::VRSkeletalMotionRange_WithoutController, m_boneTransform, HSB_Count);
}
