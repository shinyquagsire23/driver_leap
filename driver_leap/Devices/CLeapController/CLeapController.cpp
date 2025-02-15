#include "stdafx.h"

#include "Devices/CLeapController/CLeapController.h"
#include "Devices/CLeapController/CControllerButton.h"

#include "Core/CDriverConfig.h"
#include "Utils/Utils.h"

const glm::quat g_reverseRotation(0.f, 0.f, 0.70106769f, -0.70106769f);
const glm::quat g_rotateHalfPiZ(0.70106769f, 0.f, 0.f, 0.70106769f);
const glm::quat g_rotateHalfPiY(0.70106769f, 0.f, 0.70106769f, 0.0f);
const glm::quat g_rotateHalfPiZN(0.70106769f, 0.f, 0.f, -0.70106769f);
const vr::HmdQuaternion_t g_vrZeroRotation = { 1.0, .0, .0, .0 };

extern const glm::mat4 g_identityMatrix;
extern const glm::vec4 g_zeroPoint;

double CLeapController::ms_headPosition[] = { .0, .0, .0 };
vr::HmdQuaternion_t CLeapController::ms_headRotation = { 1.0, .0, .0, .0 };
glm::mat4 CLeapController::ms_world_transform;

CLeapController::CLeapController()
{
    m_propertyContainer = vr::k_ulInvalidPropertyContainer;
    m_trackedDevice = vr::k_unTrackedDeviceIndexInvalid;
    m_haptic = vr::k_ulInvalidPropertyContainer;

    m_pose = { 0 };
    m_pose.deviceIsConnected = false;
    for(size_t i = 0U; i < 3U; i++)
    {
        m_pose.vecAcceleration[i] = .0;
        m_pose.vecAngularAcceleration[i] = .0;
        m_pose.vecAngularVelocity[i] = .0;
        m_pose.vecDriverFromHeadTranslation[i] = .0;
    }
    m_pose.poseTimeOffset = .0;
    m_pose.qDriverFromHeadRotation = g_vrZeroRotation;
    m_pose.qRotation = g_vrZeroRotation;
    m_pose.qWorldFromDriverRotation = g_vrZeroRotation;
    m_pose.result = vr::TrackingResult_Uninitialized;
    m_pose.shouldApplyHeadModel = false;
    m_pose.willDriftInYaw = false;

    m_hand = CH_Left;
    m_type = CT_Invalid;
}

CLeapController::~CLeapController()
{
    for(auto l_button : m_buttons) delete l_button;
}

// vr::ITrackedDeviceServerDriver
vr::EVRInitError CLeapController::Activate(uint32_t unObjectId)
{
    vr::EVRInitError l_resultError = vr::VRInitError_Driver_Failed;

    if(m_trackedDevice == vr::k_unTrackedDeviceIndexInvalid)
    {
        m_trackedDevice = unObjectId;
        m_propertyContainer = vr::VRProperties()->TrackedDeviceToPropertyContainer(m_trackedDevice);

        ActivateInternal();

        l_resultError = vr::VRInitError_None;
    }

    return l_resultError;
}

void CLeapController::Deactivate()
{
    ResetControls();
    m_trackedDevice = vr::k_unTrackedDeviceIndexInvalid;
}

void CLeapController::EnterStandby()
{
}

void* CLeapController::GetComponent(const char* pchComponentNameAndVersion)
{
    void *l_result = nullptr;
    if(!strcmp(pchComponentNameAndVersion, vr::ITrackedDeviceServerDriver_Version)) l_result = dynamic_cast<vr::ITrackedDeviceServerDriver*>(this);
    return l_result;
}

void CLeapController::DebugRequest(const char* pchRequest, char* pchResponseBuffer, uint32_t unResponseBufferSize)
{
}

vr::DriverPose_t CLeapController::GetPose()
{
    return m_pose;
}

// CLeapController
bool CLeapController::IsEnabled() const
{
    return m_isEnabled;
}

void CLeapController::SetEnabled(bool f_state)
{
    m_isEnabled = f_state;
    m_pose.deviceIsConnected = f_state;
    if(m_trackedDevice != vr::k_unTrackedDeviceIndexInvalid) vr::VRServerDriverHost()->TrackedDevicePoseUpdated(m_trackedDevice, m_pose, sizeof(vr::DriverPose_t));
}

const std::string& CLeapController::GetSerialNumber() const
{
    return m_serialNumber;
}

void CLeapController::ResetControls()
{
    for(auto l_button : m_buttons)
    {
        l_button->SetValue(0.f);
        l_button->SetState(false);
    }
}

void CLeapController::RunFrame(const LEAP_HAND *f_hand, const LEAP_HAND *f_oppHand)
{
    if(m_trackedDevice != vr::k_unTrackedDeviceIndexInvalid)
    {
        if(m_isEnabled)
        {
            UpdateTransformation(f_hand);
            UpdateInputInterop();
            vr::VRServerDriverHost()->TrackedDevicePoseUpdated(m_trackedDevice, m_pose, sizeof(vr::DriverPose_t));

            UpdateGestures(f_hand, f_oppHand);
            UpdateInput();
        }
        else vr::VRServerDriverHost()->TrackedDevicePoseUpdated(m_trackedDevice, m_pose, sizeof(vr::DriverPose_t));
    }
}

void CLeapController::UpdateInput()
{
    for(auto l_button : m_buttons)
    {
        if(l_button->IsUpdated())
        {
            switch(l_button->GetInputType())
            {
                case CControllerButton::IT_Boolean:
                    vr::VRDriverInput()->UpdateBooleanComponent(l_button->GetHandle(), l_button->GetState(), .0);
                    break;
                case CControllerButton::IT_Float:
                    vr::VRDriverInput()->UpdateScalarComponent(l_button->GetHandle(), l_button->GetValue(), .0);
                    break;
            }
            l_button->ResetUpdate();
        }
    }
    UpdateInputInternal();
}

void CLeapController::UpdateTransformation(const LEAP_HAND *f_hand)
{
    m_pose.poseIsValid = (f_hand != nullptr);

    if(f_hand)
    {
        //std::memcpy(m_pose.vecWorldFromDriverTranslation, ms_headPosition, sizeof(double) * 3U);

        const LEAP_VECTOR l_palmPosition = f_hand->palm.position;
        const LEAP_VECTOR l_wristPosition = f_hand->arm.next_joint;
        const LEAP_QUATERNION l_palmOrientation = f_hand->palm.orientation;

        switch(CDriverConfig::GetOrientationMode())
        {
            case CDriverConfig::OM_HMD:
            {
                //std::memcpy(&m_pose.qWorldFromDriverRotation, &ms_headRotation, sizeof(vr::HmdQuaternion_t));



                const glm::vec3 &l_handOffset = ((m_hand == CH_Left) ? CDriverConfig::GetLeftHandOffset() : CDriverConfig::GetRightHandOffset());
                m_pose.vecPosition[0] = -0.001f* l_wristPosition.x + l_handOffset.x;
                m_pose.vecPosition[1] = -0.001f* l_wristPosition.z + l_handOffset.y;
                m_pose.vecPosition[2] = -0.001f* l_wristPosition.y + l_handOffset.z;

                const glm::quat l_headRotation(ms_headRotation.w, ms_headRotation.x, ms_headRotation.y, ms_headRotation.z);

                if(CDriverConfig::IsVelocityUsed())
                {
                    const LEAP_VECTOR l_palmVelocity = f_hand->palm.velocity;
                    glm::vec3 l_resultVelocity(-0.001f*l_palmVelocity.x, -0.001f*l_palmVelocity.z, -0.001f*l_palmVelocity.y);
                    
                    l_resultVelocity = l_headRotation*l_resultVelocity;
                    m_pose.vecVelocity[0] = l_resultVelocity.x;
                    m_pose.vecVelocity[1] = l_resultVelocity.y;
                    m_pose.vecVelocity[2] = l_resultVelocity.z;
                }

                glm::quat l_rotation(l_palmOrientation.w, l_palmOrientation.x, l_palmOrientation.y, l_palmOrientation.z);
                l_rotation = g_reverseRotation*l_rotation;
                //l_rotation *= ((m_hand == CH_Left) ? g_rotateHalfPiZN : g_rotateHalfPiZ);
                //l_rotation *= ((m_hand == CH_Left) ? g_rotateHalfPiZN : g_rotateHalfPiZ);

                if (m_hand == CH_Left) {
                    //l_rotation *= g_rotateHalfPiY;
                    //l_rotation *= g_rotateHalfPiY;
                }

                l_rotation *= ((m_hand == CH_Left) ? CDriverConfig::GetLeftHandOffsetRotation() : CDriverConfig::GetRightHandOffsetRotation());

                glm::vec3 tmpPos = glm::vec3(m_pose.vecPosition[0], m_pose.vecPosition[1], m_pose.vecPosition[2]);
                tmpPos = ms_world_transform * glm::translate(g_identityMatrix, tmpPos) * g_zeroPoint;
                m_pose.vecPosition[0] = tmpPos.x;
                m_pose.vecPosition[1] = tmpPos.y;
                m_pose.vecPosition[2] = tmpPos.z;

                l_rotation = l_headRotation*l_rotation;

                m_pose.qRotation.x = l_rotation.x;
                m_pose.qRotation.y = l_rotation.y;
                m_pose.qRotation.z = l_rotation.z;
                m_pose.qRotation.w = l_rotation.w;
            } break;
            case CDriverConfig::OM_Desktop:
            {
                // Controller follows HMD position only
                std::memcpy(&m_pose.qWorldFromDriverRotation, &g_vrZeroRotation, sizeof(vr::HmdQuaternion_t));

                const glm::vec3 &l_offset = CDriverConfig::GetDesktopOffset();
                m_pose.vecWorldFromDriverTranslation[0U] += l_offset.x;
                m_pose.vecWorldFromDriverTranslation[1U] += l_offset.y;
                m_pose.vecWorldFromDriverTranslation[2U] += l_offset.z;

                const glm::vec3 &l_handOffset = ((m_hand == CH_Left) ? CDriverConfig::GetLeftHandOffset() : CDriverConfig::GetRightHandOffset());
                m_pose.vecPosition[0] = 0.001f*l_palmPosition.x + l_handOffset.x;
                m_pose.vecPosition[1] = 0.001f*l_palmPosition.y + l_handOffset.y;
                m_pose.vecPosition[2] = 0.001f*l_palmPosition.z + l_handOffset.z;

                if(CDriverConfig::IsVelocityUsed())
                {
                    const LEAP_VECTOR l_palmVelocity = f_hand->palm.velocity;
                    m_pose.vecVelocity[0] = 0.001f*l_palmVelocity.x;
                    m_pose.vecVelocity[1] = 0.001f*l_palmVelocity.y;
                    m_pose.vecVelocity[2] = 0.001f*l_palmVelocity.z;
                }

                glm::quat l_rotation(l_palmOrientation.w, l_palmOrientation.x, l_palmOrientation.y, l_palmOrientation.z);
                l_rotation *= ((m_hand == CH_Left) ? g_rotateHalfPiZN : g_rotateHalfPiZ);
                l_rotation *= ((m_hand == CH_Left) ? CDriverConfig::GetLeftHandOffsetRotation() : CDriverConfig::GetRightHandOffsetRotation());

                m_pose.qRotation.x = l_rotation.x;
                m_pose.qRotation.y = l_rotation.y;
                m_pose.qRotation.z = l_rotation.z;
                m_pose.qRotation.w = l_rotation.w;
            } break;
        }
        m_pose.result = vr::TrackingResult_Running_OK;
    }
    else
    {
        for(size_t i = 0U; i < 3U; i++) m_pose.vecVelocity[i] = .0;
        if(CDriverConfig::IsHandsResetEnabled()) m_pose.result = vr::TrackingResult_Running_OutOfRange;
        else
        {
            m_pose.result = vr::TrackingResult_Running_OK;
            m_pose.poseIsValid = true;
        }
    }
}

void CLeapController::ActivateInternal()
{
}

void CLeapController::UpdateGestures(const LEAP_HAND *f_hand, const LEAP_HAND *f_oppHand)
{
}

void CLeapController::UpdateInputInternal()
{
    
}

void CLeapController::UpdateInputInterop()
{
    if (!m_pose.poseIsValid) return;

    vr::TrackedDevicePose_t device_poses[32];
    vr::VRServerDriverHost()->GetRawTrackedDevicePoses(0, device_poses, 32);

    glm::mat4 l_rotMat(1.f);
    ConvertMatrix(device_poses[0].mDeviceToAbsoluteTracking, l_rotMat);

    glm::vec3 pose_abs(m_pose.vecPosition[0], m_pose.vecPosition[1], m_pose.vecPosition[2]);

    int controllerRole = (m_hand == CH_Left) ? vr::TrackedControllerRole_LeftHand : vr::TrackedControllerRole_RightHand;
    int controllerPrio = 0x7FFFFFF; // higher than everything
    m_pose.deviceIsConnected = true;

    for (int i = 0; i < 32; i++)
    {
        auto props = vr::VRProperties()->TrackedDeviceToPropertyContainer(i);

        int role = vr::VRProperties()->GetInt32Property(props, vr::Prop_ControllerRoleHint_Int32);
        auto resourceRoot = vr::VRProperties()->GetStringProperty(props, vr::Prop_ResourceRoot_String);

        if (resourceRoot == "leap") continue;

        glm::vec3 device_pose(device_poses[i].mDeviceToAbsoluteTracking.m[0][3], device_poses[i].mDeviceToAbsoluteTracking.m[1][3], device_poses[i].mDeviceToAbsoluteTracking.m[2][3]);

        /*if (m_hand == CH_Right && role == vr::TrackedControllerRole_LeftHand && fabs(glm::distance(device_pose, pose_abs)) < 0.25)
        {
            m_pose.vecPosition[0] = device_pose.x;
            m_pose.vecPosition[1] = device_pose.y;
            m_pose.vecPosition[2] = device_pose.z;
        }*/

        if (m_hand == CH_Left && (role == vr::TrackedControllerRole_LeftHand || role == vr::TrackedControllerRole_OptOut))
        {
            if (fabs(glm::distance(device_pose, pose_abs)) < 0.28) {
                controllerRole = vr::TrackedControllerRole_Invalid;
                controllerPrio = 0x7FFFFFF; // set lower
                m_pose.result = vr::TrackingResult_Running_OutOfRange;
                m_pose.poseIsValid = false;
                m_pose.deviceIsConnected = false;

                vr::VRProperties()->SetInt32Property(props, vr::Prop_ControllerRoleHint_Int32, vr::TrackedControllerRole_LeftHand);
                //vr::VRProperties()->SetInt32Property(m_trackedDevice, vr::Prop_ControllerRoleHint_Int32, vr::TrackedControllerRole_OptOut);
            }
            else {
                //vr::VRProperties()->SetInt32Property(m_trackedDevice, vr::Prop_ControllerRoleHint_Int32, vr::TrackedControllerRole_LeftHand);
                //vr::VRProperties()->SetInt32Property(props, vr::Prop_ControllerRoleHint_Int32, vr::TrackedControllerRole_OptOut);
            }
        }
        else if (m_hand == CH_Right && (role == vr::TrackedControllerRole_RightHand || role == vr::TrackedControllerRole_OptOut))
        {
            if (fabs(glm::distance(device_pose, pose_abs)) < 0.28) {
                controllerRole = vr::TrackedControllerRole_Invalid;
                controllerPrio = 0x7FFFFFF; // set lower
                m_pose.result = vr::TrackingResult_Running_OutOfRange;
                m_pose.poseIsValid = false;
                m_pose.deviceIsConnected = false;

                vr::VRProperties()->SetInt32Property(props, vr::Prop_ControllerRoleHint_Int32, vr::TrackedControllerRole_RightHand);
                //vr::VRProperties()->SetInt32Property(m_trackedDevice, vr::Prop_ControllerRoleHint_Int32, vr::TrackedControllerRole_OptOut);
            }
            else {
                //vr::VRProperties()->SetInt32Property(m_trackedDevice, vr::Prop_ControllerRoleHint_Int32, vr::TrackedControllerRole_RightHand);
                //vr::VRProperties()->SetInt32Property(props, vr::Prop_ControllerRoleHint_Int32, vr::TrackedControllerRole_OptOut);
            }
        }
    }

    vr::VRProperties()->SetInt32Property(m_propertyContainer, vr::Prop_ControllerRoleHint_Int32, controllerRole);
    vr::VRProperties()->SetInt32Property(m_propertyContainer, vr::Prop_ControllerHandSelectionPriority_Int32, controllerPrio);
}

void CLeapController::UpdateHMDCoordinates()
{
    vr::TrackedDevicePose_t l_hmdPose;
    vr::VRServerDriverHost()->GetRawTrackedDevicePoses(0.f, &l_hmdPose, 1U); // HMD has device ID 0
    if(l_hmdPose.bPoseIsValid)
    {
        ConvertMatrix(l_hmdPose.mDeviceToAbsoluteTracking, ms_world_transform);

        const glm::quat l_headRot = glm::quat_cast(ms_world_transform);
        ms_headRotation.x = l_headRot.x;
        ms_headRotation.y = l_headRot.y;
        ms_headRotation.z = l_headRot.z;
        ms_headRotation.w = l_headRot.w;

        for(size_t i = 0U; i < 3U; i++) ms_headPosition[i] = l_hmdPose.mDeviceToAbsoluteTracking.m[i][3];
    }
}
