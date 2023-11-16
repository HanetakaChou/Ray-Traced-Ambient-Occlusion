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

#ifndef _USER_INPUT_MODEL_H_
#define _USER_INPUT_MODEL_H_ 1

struct ui_model_t
{
    float m_ambient_occlusion_max_distance;
    int m_ambient_occlusion_sample_count;
};

struct user_camera_model_t
{
    float m_eye_position[3];
    float m_eye_direction[3];
    float m_up_direction[3];
    float m_fov;
    float m_near;
    float m_far;
};

#endif