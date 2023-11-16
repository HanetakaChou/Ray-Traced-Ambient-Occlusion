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

#ifndef _RENDERER_H_
#define _RENDERER_H_ 1

#include "user_input_model.h"

class renderer
{
public:
    virtual void attach_window(void *wsi_window) = 0;
    virtual void on_window_resize() = 0;
    virtual void detach_window() = 0;
    virtual void draw(float interval_time, ui_model_t const *ui_model, user_camera_model_t const *user_camera_model, void const *ui_draw_data) = 0;
};

extern renderer *renderer_init(void *wsi_connection, ui_model_t *out_ui_model, user_camera_model_t *out_user_camera_model);
extern void renderer_destroy(renderer *renderer);

#endif