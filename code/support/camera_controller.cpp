//
// Copyright (C) YuqiaoZhang(HanetakaChou)
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.
//

#include <cmath>
#include <algorithm>
#include "camera_controller.h"

class CameraController g_camera_controller;

static float g_SpeedTranslate = 0.5F;
static float g_RotationTranslate = 2.0F;

CameraController::CameraController() : m_previous_x(-1.0F), m_previous_y(-1.0F)
{
}

void CameraController::MoveForward()
{
	m_eye_position.x += m_eye_direction.x * g_SpeedTranslate;
	m_eye_position.y += m_eye_direction.y * g_SpeedTranslate;
	m_eye_position.z += m_eye_direction.z * g_SpeedTranslate;
}

void CameraController::MoveBack()
{
	m_eye_position.x -= m_eye_direction.x * g_SpeedTranslate;
	m_eye_position.y -= m_eye_direction.y * g_SpeedTranslate;
	m_eye_position.z -= m_eye_direction.z * g_SpeedTranslate;
}

void CameraController::MoveLeft()
{
	DirectX::XMFLOAT3 EyeDirection = {m_eye_direction.x, m_eye_direction.y, m_eye_direction.z};
	DirectX::XMFLOAT3 UpDirection = {m_up_direction.x, m_up_direction.y, m_up_direction.z};

	DirectX::XMVECTOR NegEyeDirection = DirectX::XMVectorNegate(DirectX::XMLoadFloat3(&EyeDirection));
	DirectX::XMVECTOR AxisForwardDirection = DirectX::XMVector3Normalize(NegEyeDirection);
	DirectX::XMVECTOR AxisRightDirection = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(DirectX::XMLoadFloat3(&UpDirection), AxisForwardDirection));
	// DirectX::XMVECTOR AxisViewUpDirection = DirectX::XMVector3Cross(AxisForwardDirection, AxisRightDirection);

	DirectX::XMFLOAT3 AxisRightDirectionF;
	DirectX::XMStoreFloat3(&AxisRightDirectionF, AxisRightDirection);

	m_eye_position.x -= AxisRightDirectionF.x * g_SpeedTranslate;
	m_eye_position.y -= AxisRightDirectionF.y * g_SpeedTranslate;
	m_eye_position.z -= AxisRightDirectionF.z * g_SpeedTranslate;
}

void CameraController::MoveRight()
{
	DirectX::XMFLOAT3 EyeDirection = {m_eye_direction.x, m_eye_direction.y, m_eye_direction.z};
	DirectX::XMFLOAT3 UpDirection = {m_up_direction.x, m_up_direction.y, m_up_direction.z};

	DirectX::XMVECTOR NegEyeDirection = DirectX::XMVectorNegate(DirectX::XMLoadFloat3(&EyeDirection));
	DirectX::XMVECTOR AxisForwardDirection = DirectX::XMVector3Normalize(NegEyeDirection);
	DirectX::XMVECTOR AxisRightDirection = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(DirectX::XMLoadFloat3(&UpDirection), AxisForwardDirection));
	// DirectX::XMVECTOR AxisViewUpDirection = DirectX::XMVector3Cross(AxisForwardDirection, AxisRightDirection);

	DirectX::XMFLOAT3 AxisRightDirectionF;
	DirectX::XMStoreFloat3(&AxisRightDirectionF, AxisRightDirection);

	m_eye_position.x += AxisRightDirectionF.x * g_SpeedTranslate;
	m_eye_position.y += AxisRightDirectionF.y * g_SpeedTranslate;
	m_eye_position.z += AxisRightDirectionF.z * g_SpeedTranslate;
}

void CameraController::MoveUp()
{
	DirectX::XMFLOAT3 EyeDirection = {m_eye_direction.x, m_eye_direction.y, m_eye_direction.z};
	DirectX::XMFLOAT3 UpDirection = {m_up_direction.x, m_up_direction.y, m_up_direction.z};

	DirectX::XMVECTOR NegEyeDirection = DirectX::XMVectorNegate(DirectX::XMLoadFloat3(&EyeDirection));
	DirectX::XMVECTOR AxisForwardDirection = DirectX::XMVector3Normalize(NegEyeDirection);
	DirectX::XMVECTOR AxisRightDirection = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(DirectX::XMLoadFloat3(&UpDirection), AxisForwardDirection));
	DirectX::XMVECTOR AxisViewUpDirection = DirectX::XMVector3Cross(AxisForwardDirection, AxisRightDirection);

	DirectX::XMFLOAT3 AxisViewUpDirectionF;
	DirectX::XMStoreFloat3(&AxisViewUpDirectionF, AxisViewUpDirection);

	m_eye_position.x += AxisViewUpDirectionF.x * g_SpeedTranslate;
	m_eye_position.y += AxisViewUpDirectionF.y * g_SpeedTranslate;
	m_eye_position.z += AxisViewUpDirectionF.z * g_SpeedTranslate;
}

void CameraController::MoveDown()
{
	DirectX::XMFLOAT3 EyeDirection = {m_eye_direction.x, m_eye_direction.y, m_eye_direction.z};
	DirectX::XMFLOAT3 UpDirection = {m_up_direction.x, m_up_direction.y, m_up_direction.z};

	DirectX::XMVECTOR NegEyeDirection = DirectX::XMVectorNegate(DirectX::XMLoadFloat3(&EyeDirection));
	DirectX::XMVECTOR AxisForwardDirection = DirectX::XMVector3Normalize(NegEyeDirection);
	DirectX::XMVECTOR AxisRightDirection = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(DirectX::XMLoadFloat3(&UpDirection), AxisForwardDirection));
	DirectX::XMVECTOR AxisViewUpDirection = DirectX::XMVector3Cross(AxisForwardDirection, AxisRightDirection);

	DirectX::XMFLOAT3 AxisViewUpDirectionF;
	DirectX::XMStoreFloat3(&AxisViewUpDirectionF, AxisViewUpDirection);

	m_eye_position.x -= AxisViewUpDirectionF.x * g_SpeedTranslate;
	m_eye_position.y -= AxisViewUpDirectionF.y * g_SpeedTranslate;
	m_eye_position.z -= AxisViewUpDirectionF.z * g_SpeedTranslate;
}

void CameraController::OnMouseMove(float x, float y, bool hold)
{
	float Current_X = std::min(std::max(0.0f, x), 1.0f);
	float Current_Y = std::min(std::max(0.0f, y), 1.0f);

	float Offset_X = Current_X - this->m_previous_x;
	float Offset_Y = Current_Y - this->m_previous_y;

	if (hold && this->m_previous_x > 0.0F && this->m_previous_x < 1.0F && this->m_previous_y > 0.0F && this->m_previous_y < 1.0F)
	{
		float LengthNormalized = ::sqrtf(Offset_X * Offset_X + Offset_Y * Offset_Y);

		if ((Offset_Y < Offset_X) && (Offset_Y > -Offset_X))
		{
			// right
			DirectX::XMFLOAT3 EyeDirection = {m_eye_direction.x, m_eye_direction.y, m_eye_direction.z};
			DirectX::XMFLOAT3 UpDirection = {m_up_direction.x, m_up_direction.y, m_up_direction.z};

			DirectX::XMVECTOR NegEyeDirection = DirectX::XMVectorNegate(DirectX::XMLoadFloat3(&EyeDirection));
			DirectX::XMVECTOR AxisForwardDirection = DirectX::XMVector3Normalize(NegEyeDirection);
			DirectX::XMVECTOR AxisRightDirection = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(DirectX::XMLoadFloat3(&UpDirection), AxisForwardDirection));
			DirectX::XMVECTOR AxisViewUpDirection = DirectX::XMVector3Cross(AxisForwardDirection, AxisRightDirection);

			DirectX::XMFLOAT3 EyeDirectionNew;
			DirectX::XMStoreFloat3(&EyeDirectionNew, DirectX::XMVectorNegate(DirectX::XMVector3Transform(AxisForwardDirection, DirectX::XMMatrixRotationAxis(AxisViewUpDirection, -g_RotationTranslate * LengthNormalized))));
			m_eye_direction.x = EyeDirectionNew.x;
			m_eye_direction.y = EyeDirectionNew.y;
			m_eye_direction.z = EyeDirectionNew.z;
		}
		else if ((Offset_Y < -Offset_X) && (Offset_Y > Offset_X))
		{
			// left
			DirectX::XMFLOAT3 EyeDirection = {m_eye_direction.x, m_eye_direction.y, m_eye_direction.z};
			DirectX::XMFLOAT3 UpDirection = {m_up_direction.x, m_up_direction.y, m_up_direction.z};

			DirectX::XMVECTOR NegEyeDirection = DirectX::XMVectorNegate(DirectX::XMLoadFloat3(&EyeDirection));
			DirectX::XMVECTOR AxisForwardDirection = DirectX::XMVector3Normalize(NegEyeDirection);
			DirectX::XMVECTOR AxisRightDirection = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(DirectX::XMLoadFloat3(&UpDirection), AxisForwardDirection));
			DirectX::XMVECTOR AxisViewUpDirection = DirectX::XMVector3Cross(AxisForwardDirection, AxisRightDirection);

			DirectX::XMFLOAT3 EyeDirectionNew;
			DirectX::XMStoreFloat3(&EyeDirectionNew, DirectX::XMVectorNegate(DirectX::XMVector3Transform(AxisForwardDirection, DirectX::XMMatrixRotationAxis(AxisViewUpDirection, g_RotationTranslate * LengthNormalized))));
			m_eye_direction.x = EyeDirectionNew.x;
			m_eye_direction.y = EyeDirectionNew.y;
			m_eye_direction.z = EyeDirectionNew.z;
		}
		else if ((Offset_Y < Offset_X) && (Offset_Y < -Offset_X))
		{
			// up
			DirectX::XMFLOAT3 EyeDirection = {m_eye_direction.x, m_eye_direction.y, m_eye_direction.z};
			DirectX::XMFLOAT3 UpDirection = {m_up_direction.x, m_up_direction.y, m_up_direction.z};

			DirectX::XMVECTOR NegEyeDirection = DirectX::XMVectorNegate(DirectX::XMLoadFloat3(&EyeDirection));
			DirectX::XMVECTOR AxisForwardDirection = DirectX::XMVector3Normalize(NegEyeDirection);
			DirectX::XMVECTOR AxisRightDirection = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(DirectX::XMLoadFloat3(&UpDirection), AxisForwardDirection));
			// DirectX::XMVECTOR AxisViewUpDirection = DirectX::XMVector3Cross(AxisForwardDirection, AxisRightDirection);

			DirectX::XMFLOAT3 EyeDirectionNew;
			DirectX::XMStoreFloat3(&EyeDirectionNew, DirectX::XMVectorNegate(DirectX::XMVector3Transform(AxisForwardDirection, DirectX::XMMatrixRotationAxis(AxisRightDirection, g_RotationTranslate * LengthNormalized))));
			m_eye_direction.x = EyeDirectionNew.x;
			m_eye_direction.y = EyeDirectionNew.y;
			m_eye_direction.z = EyeDirectionNew.z;
		}
		else if ((Offset_Y > Offset_X) && (Offset_Y > -Offset_X))
		{
			// down
			DirectX::XMFLOAT3 EyeDirection = {m_eye_direction.x, m_eye_direction.y, m_eye_direction.z};
			DirectX::XMFLOAT3 UpDirection = {m_up_direction.x, m_up_direction.y, m_up_direction.z};

			DirectX::XMVECTOR NegEyeDirection = DirectX::XMVectorNegate(DirectX::XMLoadFloat3(&EyeDirection));
			DirectX::XMVECTOR AxisForwardDirection = DirectX::XMVector3Normalize(NegEyeDirection);
			DirectX::XMVECTOR AxisRightDirection = DirectX::XMVector3Normalize(DirectX::XMVector3Cross(DirectX::XMLoadFloat3(&UpDirection), AxisForwardDirection));
			// DirectX::XMVECTOR AxisViewUpDirection = DirectX::XMVector3Cross(AxisForwardDirection, AxisRightDirection);

			DirectX::XMFLOAT3 EyeDirectionNew;
			DirectX::XMStoreFloat3(&EyeDirectionNew, DirectX::XMVectorNegate(DirectX::XMVector3Transform(AxisForwardDirection, DirectX::XMMatrixRotationAxis(AxisRightDirection, -g_RotationTranslate * LengthNormalized))));
			m_eye_direction.x = EyeDirectionNew.x;
			m_eye_direction.y = EyeDirectionNew.y;
			m_eye_direction.z = EyeDirectionNew.z;
		}
	}

	this->m_previous_x = Current_X;
	this->m_previous_y = Current_Y;
}
