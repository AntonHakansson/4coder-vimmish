// TOP

#if !defined(FCODER_DEFAULT_BINDINGS_CPP)
#define FCODER_DEFAULT_BINDINGS_CPP

#include "4coder_default_include.cpp"

CUSTOM_ID(command_map, snd_mapid_shared);
CUSTOM_ID(command_map, snd_mapid_command_mode);
CUSTOM_ID(attachment, snd_buffer_attachment);

#include "generated/managed_id_metadata.cpp"

#define cast(...) (__VA_ARGS__)

struct Snd_Motion_Result {
    Range_i64 range;
    i64 seek_pos;
};

inline Snd_Motion_Result snd_null_motion(i64 pos) {
    Snd_Motion_Result result = { Ii64(pos, pos), pos };
    return result;
}

enum Snd_Text_Motion {
    SndTextMotion_None,

    SndTextMotion_Word,
    SndTextMotion_WordEnd,
    SndTextMotion_LineSide,
    SndTextMotion_ScopeVimStyle,
    SndTextMotion_Scope4CoderStyle,
    SndTextMotion_SurroundingScope,
};

enum Snd_Text_Action {
    SndTextAction_None,

    SndTextAction_Cut,
    SndTextAction_Change,
};

struct Snd_Text_Command {
    History_Record_Index history_record_index;

    Snd_Text_Action action;
    Snd_Text_Motion motion;
    Scan_Direction motion_direction;
    i32 motion_count;
};

inline b32 snd_text_command_is_valid(Snd_Text_Command command) {
    b32 result = (command.motion != SndTextMotion_None && command.motion_count > 0);
    return result;
}

enum Snd_Window_Action {
    SndWindowAction_None,

    SndWindowAction_Cycle,
    SndWindowAction_Left,
    SndWindowAction_Right,
    SndWindowAction_Up,
    SndWindowAction_Down,
    SndWindowAction_Swap,
    SndWindowAction_VSplit,
    SndWindowAction_HSplit,
};

Snd_Text_Command snd_most_recent_text_command;

enum Snd_Buffer_Flag {
    SndBufferFlag_TreatAsCode = 0x1,
    SndBufferFlag_Append = 0x2,
    SndBufferFlag_BegunHistoryGroup = 0x4,
};

#if 0
struct SndCommandNode {
    SndCommandNode* next;
    Custom_Command_Function* f;
};

global b32 snd_recording_input;
global SndCommandNode* snd_first_command_node;
global SndCommandNode* snd_first_free_command_node;
#endif

struct Snd_Buffer_Attachment {
    u32 flags;
    i64 cursor_pos_on_append;

    History_Group history;
};

//
// @Note: Hooks and such
//

function void snd_draw_cursor(Application_Links *app, View_ID view_id, b32 is_active_view, Buffer_ID buffer, Text_Layout_ID text_layout_id, f32 roundness, f32 outline_thickness, b32 in_command_mode) {
    b32 has_highlight_range = draw_highlight_range(app, view_id, buffer, text_layout_id, roundness);
    if (!has_highlight_range) {
        i32 cursor_sub_id = default_cursor_sub_id();

        i64 cursor_pos = view_get_cursor_pos(app, view_id);
        i64 mark_pos = view_get_mark_pos(app, view_id);
        if (is_active_view) {
            if (in_command_mode) {
                draw_character_block(app, text_layout_id, cursor_pos, roundness, fcolor_id(defcolor_cursor, cursor_sub_id));
                paint_text_color_pos(app, text_layout_id, cursor_pos, fcolor_id(defcolor_at_cursor));
                draw_character_wire_frame(app, text_layout_id, mark_pos, roundness, outline_thickness, fcolor_id(defcolor_mark));
            } else {
                draw_character_i_bar(app, text_layout_id, cursor_pos, fcolor_id(defcolor_cursor, cursor_sub_id));
                draw_character_wire_frame(app, text_layout_id, mark_pos, roundness, outline_thickness, fcolor_id(defcolor_mark));
            }
        } else {
            /* Draw nothing at all... */
        }
    }
}

function void snd_render_buffer(Application_Links *app, View_ID view_id, Face_ID face_id, Buffer_ID buffer, Text_Layout_ID text_layout_id, Rect_f32 rect) {
    ProfileScope(app, "render buffer");

    View_ID active_view = get_active_view(app, Access_Always);
    b32 is_active_view = (active_view == view_id);
    Rect_f32 prev_clip = draw_set_clip(app, rect);

    // NOTE(allen): Token colorizing
    Token_Array token_array = get_token_array_from_buffer(app, buffer);
    if (token_array.tokens != 0){
        draw_cpp_token_colors(app, text_layout_id, &token_array);

        // NOTE(allen): Scan for TODOs and NOTEs
        if (global_config.use_comment_keyword){
            Comment_Highlight_Pair pairs[] = {
                {string_u8_litexpr("NOTE"), finalize_color(defcolor_comment_pop, 0)},
                {string_u8_litexpr("TODO"), finalize_color(defcolor_comment_pop, 1)},
            };
            draw_comment_highlights(app, buffer, text_layout_id, &token_array, pairs, ArrayCount(pairs));
        }
    }
    else{
        Range_i64 visible_range = text_layout_get_visible_range(app, text_layout_id);
        paint_text_color_fcolor(app, text_layout_id, visible_range, fcolor_id(defcolor_text_default));
    }

    i64 cursor_pos = view_correct_cursor(app, view_id);
    view_correct_mark(app, view_id);

    // NOTE(allen): Scope highlight
    if (global_config.use_scope_highlight){
        Color_Array colors = finalize_color_array(defcolor_back_cycle);
        draw_scope_highlight(app, buffer, text_layout_id, cursor_pos, colors.vals, colors.count);
    }

    if (global_config.use_error_highlight || global_config.use_jump_highlight){
        // NOTE(allen): Error highlight
        String_Const_u8 name = string_u8_litexpr("*compilation*");
        Buffer_ID compilation_buffer = get_buffer_by_name(app, name, Access_Always);
        if (global_config.use_error_highlight){
            draw_jump_highlights(app, buffer, text_layout_id, compilation_buffer, fcolor_id(defcolor_highlight_junk));
        }

        // NOTE(allen): Search highlight
        if (global_config.use_jump_highlight){
            Buffer_ID jump_buffer = get_locked_jump_buffer(app);
            if (jump_buffer != compilation_buffer){
                draw_jump_highlights(app, buffer, text_layout_id, jump_buffer, fcolor_id(defcolor_highlight_white));
            }
        }
    }

    // NOTE(allen): Color parens
    if (global_config.use_paren_helper){
        Color_Array colors = finalize_color_array(defcolor_text_cycle);
        draw_paren_highlight(app, buffer, text_layout_id, cursor_pos, colors.vals, colors.count);
    }

    // NOTE(allen): Line highlight
    if (global_config.highlight_line_at_cursor && is_active_view){
        i64 line_number = get_line_number_from_pos(app, buffer, cursor_pos);
        draw_line_highlight(app, text_layout_id, line_number, fcolor_id(defcolor_highlight_cursor_line));
    }

    // NOTE(allen): Cursor shape
    Face_Metrics metrics = get_face_metrics(app, face_id);
    f32 cursor_roundness = (metrics.normal_advance*0.5f)*0.9f;
    f32 mark_thickness = 2.f;

    // NOTE(allen): Cursor
    // NOTE(snd): But do it my way!!!
    Managed_Scope scope = buffer_get_managed_scope(app, buffer);
    Command_Map_ID* map_id_ptr = scope_attachment(app, scope, buffer_map_id, Command_Map_ID);
    b32 in_command_mode = (*map_id_ptr) == cast(i64) snd_mapid_command_mode; // @TODO: Weird!!! Why is Command_Map_ID i64 but Managed_ID u64?

    snd_draw_cursor(app, view_id, is_active_view, buffer, text_layout_id, cursor_roundness, mark_thickness, in_command_mode);

    // NOTE(allen): Fade ranges
    paint_fade_ranges(app, text_layout_id, buffer, view_id);

    // NOTE(allen): put the actual text on the actual screen
    draw_text_layout_default(app, text_layout_id);

    draw_set_clip(app, prev_clip);
}

function void snd_render_caller(Application_Links *app, Frame_Info frame_info, View_ID view_id) {
    ProfileScope(app, "default render caller");
    View_ID active_view = get_active_view(app, Access_Always);
    b32 is_active_view = (active_view == view_id);

    Rect_f32 region = draw_background_and_margin(app, view_id, is_active_view);
    Rect_f32 prev_clip = draw_set_clip(app, region);

    Buffer_ID buffer = view_get_buffer(app, view_id, Access_Always);
    Face_ID face_id = get_face_id(app, buffer);
    Face_Metrics face_metrics = get_face_metrics(app, face_id);
    f32 line_height = face_metrics.line_height;
    f32 digit_advance = face_metrics.decimal_digit_advance;

    // NOTE(allen): file bar
    b64 showing_file_bar = false;
    if (view_get_setting(app, view_id, ViewSetting_ShowFileBar, &showing_file_bar) && showing_file_bar){
        Rect_f32_Pair pair = layout_file_bar_on_top(region, line_height);
        draw_file_bar(app, view_id, buffer, face_id, pair.min);
        region = pair.max;
    }

    Buffer_Scroll scroll = view_get_buffer_scroll(app, view_id);

    Buffer_Point_Delta_Result delta = delta_apply(app, view_id,
                                                  frame_info.animation_dt, scroll);
    if (!block_match_struct(&scroll.position, &delta.point)){
        block_copy_struct(&scroll.position, &delta.point);
        view_set_buffer_scroll(app, view_id, scroll, SetBufferScroll_NoCursorChange);
    }
    if (delta.still_animating){
        animate_in_n_milliseconds(app, 0);
    }

    // NOTE(allen): query bars
    {
        Query_Bar *space[32];
        Query_Bar_Ptr_Array query_bars = {};
        query_bars.ptrs = space;
        if (get_active_query_bars(app, view_id, ArrayCount(space), &query_bars)){
            for (i32 i = 0; i < query_bars.count; i += 1){
                Rect_f32_Pair pair = layout_query_bar_on_top(region, line_height, 1);
                draw_query_bar(app, query_bars.ptrs[i], face_id, pair.min);
                region = pair.max;
            }
        }
    }

    // NOTE(allen): FPS hud
    if (show_fps_hud){
        Rect_f32_Pair pair = layout_fps_hud_on_bottom(region, line_height);
        draw_fps_hud(app, frame_info, face_id, pair.max);
        region = pair.min;
        animate_in_n_milliseconds(app, 1000);
    }

    // NOTE(allen): layout line numbers
    Rect_f32 line_number_rect = {};
    if (global_config.show_line_number_margins){
        Rect_f32_Pair pair = layout_line_number_margin(app, buffer, region, digit_advance);
        line_number_rect = pair.min;
        region = pair.max;
    }

    // NOTE(allen): begin buffer render
    Buffer_Point buffer_point = scroll.position;
    Text_Layout_ID text_layout_id = text_layout_create(app, buffer, region, buffer_point);

    // NOTE(allen): draw line numbers
    if (global_config.show_line_number_margins){
        draw_line_number_margin(app, view_id, buffer, face_id, text_layout_id, line_number_rect);
    }

    // NOTE(allen): draw the buffer
    // NOTE(snd): but do it my way!!!
    snd_render_buffer(app, view_id, face_id, buffer, text_layout_id, region);

    text_layout_free(app, text_layout_id);
    draw_set_clip(app, prev_clip);
}

BUFFER_HOOK_SIG(snd_begin_buffer) {
    ProfileScope(app, "begin buffer");

    Scratch_Block scratch(app);

    b32 treat_as_code = false;
    String_Const_u8 file_name = push_buffer_file_name(app, scratch, buffer_id);
    if (file_name.size > 0){
        String_Const_u8_Array extensions = global_config.code_exts;
        String_Const_u8 ext = string_file_extension(file_name);
        for (i32 i = 0; i < extensions.count; ++i){
            if (string_match(ext, extensions.strings[i])){
                if (string_match(ext, string_u8_litexpr("cpp")) ||
                    string_match(ext, string_u8_litexpr("h"))   ||
                    string_match(ext, string_u8_litexpr("c"))   ||
                    string_match(ext, string_u8_litexpr("hpp")) ||
                    string_match(ext, string_u8_litexpr("cc"))
                ) {
                    treat_as_code = true;
                }

                break;
            }
        }
    }

    Command_Map_ID map_id = snd_mapid_command_mode;

    Managed_Scope scope = buffer_get_managed_scope(app, buffer_id);
    Command_Map_ID *map_id_ptr = scope_attachment(app, scope, buffer_map_id, Command_Map_ID);
    *map_id_ptr = map_id;

    Snd_Buffer_Attachment* snd_buffer = scope_attachment(app, scope, snd_buffer_attachment, Snd_Buffer_Attachment);
    // TODO(snd): I don't know if scope attachments are zero initialised so better safe than sorry
    block_zero_struct(snd_buffer);
    if (treat_as_code) {
        snd_buffer->flags |= SndBufferFlag_TreatAsCode;
    }

    Line_Ending_Kind setting = guess_line_ending_kind_from_buffer(app, buffer_id);
    Line_Ending_Kind *eol_setting = scope_attachment(app, scope, buffer_eol_setting, Line_Ending_Kind);
    *eol_setting = setting;

    // NOTE(allen): Decide buffer settings
    b32 wrap_lines = true;
    b32 use_virtual_whitespace = false;
    b32 use_lexer = false;
    if (treat_as_code){
        wrap_lines = global_config.enable_code_wrapping;
        use_virtual_whitespace = global_config.enable_virtual_whitespace;
        use_lexer = true;
    }

    String_Const_u8 buffer_name = push_buffer_base_name(app, scratch, buffer_id);
    if (string_match(buffer_name, string_u8_litexpr("*compilation*"))){
        wrap_lines = false;
    }

    if (use_lexer){
        ProfileBlock(app, "begin buffer kick off lexer");
        Async_Task *lex_task_ptr = scope_attachment(app, scope, buffer_lex_task, Async_Task);
        *lex_task_ptr = async_task_no_dep(&global_async_system, do_full_lex_async, make_data_struct(&buffer_id));
    }

    {
        b32 *wrap_lines_ptr = scope_attachment(app, scope, buffer_wrap_lines, b32);
        *wrap_lines_ptr = wrap_lines;
    }

    if (use_virtual_whitespace){
        if (use_lexer){
            buffer_set_layout(app, buffer_id, layout_virt_indent_index_generic);
        }
        else{
            buffer_set_layout(app, buffer_id, layout_virt_indent_literal_generic);
        }
    }
    else{
        buffer_set_layout(app, buffer_id, layout_generic);
    }

    // no meaning for return
    return(0);
}

//
// @Note: Commands
//

CUSTOM_COMMAND_SIG(snd_select_line) {
    View_ID view = get_active_view(app, Access_ReadVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);

    i64 start_of_line = get_line_start_pos_from_pos(app, buffer, view_get_cursor_pos(app, view));

    view_set_mark(app, view, seek_pos(start_of_line));
    seek_end_of_line(app);
}

CUSTOM_COMMAND_SIG(snd_move_left_on_line) {
    View_ID view = get_active_view(app, Access_ReadVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);

    i64 cursor_pos = view_get_cursor_pos(app, view);
    i64 line_start_pos = get_line_start_pos_from_pos(app, buffer, cursor_pos);
    i64 legal_line_start_pos = view_get_character_legal_pos_from_pos(app, view, line_start_pos);

    if (cursor_pos != legal_line_start_pos) {
        move_left(app);
    }
}

CUSTOM_COMMAND_SIG(snd_move_right_on_line) {
    View_ID view = get_active_view(app, Access_ReadVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);

    i64 cursor_pos = view_get_cursor_pos(app, view);
    i64 line_end_pos = get_line_end_pos_from_pos(app, buffer, cursor_pos);
    i64 legal_line_end_pos = view_get_character_legal_pos_from_pos(app, view, line_end_pos);

    if (cursor_pos != legal_line_end_pos) {
        move_right(app);
    }
}

inline History_Group* snd_begin_history_group_for_buffer(Application_Links* app, History_Group history) {
    Managed_Scope scope = buffer_get_managed_scope(app, history.buffer);
    Snd_Buffer_Attachment* snd_buffer = scope_attachment(app, scope, snd_buffer_attachment, Snd_Buffer_Attachment);

    if (!(snd_buffer->flags & SndBufferFlag_BegunHistoryGroup)) {
        snd_buffer->history = history;
        snd_buffer->flags |= SndBufferFlag_BegunHistoryGroup;
    } else {
        print_message(app, string_u8_litexpr("Warning: Tried to open history group for buffer when there's already one open"));
    }

    return &snd_buffer->history;
}

inline History_Group* snd_begin_history_group_for_buffer(Application_Links* app, Buffer_ID buffer) {
    return snd_begin_history_group_for_buffer(app, history_group_begin(app, buffer));
}

inline void snd_end_history_group_for_buffer(Application_Links* app, Buffer_ID buffer) {
    Managed_Scope scope = buffer_get_managed_scope(app, buffer);
    Snd_Buffer_Attachment* snd_buffer = scope_attachment(app, scope, snd_buffer_attachment, Snd_Buffer_Attachment);
    if (snd_buffer->flags & SndBufferFlag_BegunHistoryGroup) {
        history_group_end(snd_buffer->history);
        snd_buffer->flags &= ~SndBufferFlag_BegunHistoryGroup;
    } else {
        print_message(app, string_u8_litexpr("Warning: Tried to end history group for buffer when there isn't one open"));
    }
}

CUSTOM_COMMAND_SIG(snd_enter_command_mode)
{
    View_ID view = get_active_view(app, Access_ReadVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);
    Managed_Scope scope = buffer_get_managed_scope(app, buffer);

    Command_Map_ID* map_id_ptr = scope_attachment(app, scope, buffer_map_id, Command_Map_ID);
    *map_id_ptr = snd_mapid_command_mode;

    Snd_Buffer_Attachment* snd_buffer = scope_attachment(app, scope, snd_buffer_attachment, Snd_Buffer_Attachment);

    if (snd_buffer->flags & SndBufferFlag_Append) {
        if (snd_buffer->cursor_pos_on_append == view_get_cursor_pos(app, view)) {
            snd_move_left_on_line(app);
        }
        snd_buffer->flags &= ~SndBufferFlag_Append;
    }

    if (snd_buffer->flags & SndBufferFlag_BegunHistoryGroup) {
        snd_end_history_group_for_buffer(app, buffer);
    }
}

CUSTOM_COMMAND_SIG(snd_exit_command_mode) {
    View_ID view = get_active_view(app, Access_ReadWriteVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWriteVisible);
    Managed_Scope scope = buffer_get_managed_scope(app, buffer);
    Command_Map_ID* map_id_ptr = scope_attachment(app, scope, buffer_map_id, Command_Map_ID);

    Snd_Buffer_Attachment* snd_buffer = scope_attachment(app, scope, snd_buffer_attachment, Snd_Buffer_Attachment);
    if (snd_buffer->flags & SndBufferFlag_TreatAsCode) {
        *map_id_ptr = mapid_code;
    } else {
        *map_id_ptr = mapid_file;
    }

    if (!(snd_buffer->flags & SndBufferFlag_BegunHistoryGroup)) {
        snd_begin_history_group_for_buffer(app, buffer);
    }

    keyboard_macro_start_recording(app);
}

internal void snd_enter_append_mode(Application_Links* app) {
    View_ID view = get_active_view(app, Access_ReadWriteVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWriteVisible);
    Managed_Scope scope = buffer_get_managed_scope(app, buffer);
    Snd_Buffer_Attachment* snd_buffer = scope_attachment(app, scope, snd_buffer_attachment, Snd_Buffer_Attachment);

    snd_buffer->cursor_pos_on_append = view_get_character_legal_pos_from_pos(app, view, view_get_cursor_pos(app, view));
    snd_buffer->flags |= SndBufferFlag_Append;

    snd_exit_command_mode(app);
}

CUSTOM_COMMAND_SIG(snd_append) {
    snd_move_right_on_line(app);
    snd_enter_append_mode(app);
}

CUSTOM_COMMAND_SIG(snd_append_to_line_end) {
    seek_end_of_line(app);
    snd_enter_append_mode(app);
}

#if 0
CUSTOM_COMMAND_SIG(snd_move_right_token_end)
{
    // @TODO: Isn't this too complex and weird?
    Scratch_Block scratch(app);

    View_ID view = get_active_view(app, Access_ReadVisible);

    i64 old_cursor_pos = view_get_cursor_pos(app, view);
    Buffer_Cursor old_cursor = view_compute_cursor(app, view, seek_pos(old_cursor_pos));
    i64 old_character = view_relative_character_from_pos(app, view, old_cursor.line, old_cursor.pos);

    current_view_scan_move(app, Scan_Forward, push_boundary_list(scratch, boundary_token));

    i64 new_cursor_pos = view_get_cursor_pos(app, view);
    i64 new_character = view_relative_character_from_pos(app, view, old_cursor.line, new_cursor_pos);

    if (new_character - old_character > 1) {
        view_set_cursor_by_character_delta(app, view, -1);
    }
}
#endif

CUSTOM_COMMAND_SIG(snd_inclusive_cut) {
    move_right(app);
    cut(app);
}

CUSTOM_COMMAND_SIG(snd_change) {
    View_ID view = get_active_view(app, Access_ReadVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);

    if (!buffer_exists(app, buffer)) {
        return;
    }

    snd_begin_history_group_for_buffer(app, buffer);

    snd_inclusive_cut(app);

    snd_exit_command_mode(app);
}

internal void snd_write_text_and_auto_indent(Application_Links* app, String_Const_u8 insert) {
    if (insert.str != 0 && insert.size > 0){
        b32 do_auto_indent = false;
        for (u64 i = 0; !do_auto_indent && i < insert.size; i += 1){
            switch (insert.str[i]){
                case ';': case ':':
                case '{': case '}':
                case '(': case ')':
                case '[': case ']':
                case '#':
                case '\n': case '\t':
                {
                    do_auto_indent = true;
                }break;
            }
        }
        if (do_auto_indent){
            View_ID view = get_active_view(app, Access_ReadWriteVisible);
            Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWriteVisible);
            Range_i64 pos = {};
            pos.min = view_get_cursor_pos(app, view);
            write_text(app, insert);
            pos.max= view_get_cursor_pos(app, view);
            auto_indent_buffer(app, buffer, pos, 0);
            move_past_lead_whitespace(app, view, buffer);
        }
        else{
            write_text(app, insert);
        }
    }
}

CUSTOM_COMMAND_SIG(snd_new_line_below) {
    View_ID view = get_active_view(app, Access_ReadWriteVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWriteVisible);

    if (!buffer_exists(app, buffer)) {
        return;
    }

    snd_begin_history_group_for_buffer(app, buffer);

    seek_end_of_line(app);
    snd_write_text_and_auto_indent(app, string_u8_litexpr("\n"));

    snd_exit_command_mode(app);
}

CUSTOM_COMMAND_SIG(snd_new_line_above) {
    View_ID view = get_active_view(app, Access_ReadWriteVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWriteVisible);

    if (!buffer_exists(app, buffer)) {
        return;
    }

    // TODO(snd): Why does this not restore the cursor to the right position?
    snd_begin_history_group_for_buffer(app, buffer);

    seek_beginning_of_line(app);
    move_left(app);
    snd_write_text_and_auto_indent(app, string_u8_litexpr("\n"));

    snd_exit_command_mode(app);
}

CUSTOM_COMMAND_SIG(snd_join_line) {
    View_ID view = get_active_view(app, Access_ReadWriteVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWriteVisible);

    if (!buffer_exists(app, buffer)) {
        return;
    }

    i64 pos = view_get_cursor_pos(app, view);
    i64 line = get_line_number_from_pos(app, buffer, pos);
    i64 line_start = get_pos_past_lead_whitespace_from_line_number(app, buffer, line);
    i64 line_end = get_line_end_pos(app, buffer, line);
    i64 next_line_start = get_pos_past_lead_whitespace_from_line_number(app, buffer, line + 1);

    // @TODO: Do language agnostic comments
    if (c_line_comment_starts_at_position(app, buffer, line_start) && c_line_comment_starts_at_position(app, buffer, next_line_start)) {
        next_line_start += 2;
        while (character_is_whitespace(buffer_get_char(app, buffer, next_line_start))) {
            next_line_start++;
        }
    }

    i64 new_pos = view_get_character_legal_pos_from_pos(app, view, line_end);
    view_set_cursor_and_preferred_x(app, view, seek_pos(new_pos));

    buffer_replace_range(app, buffer, Ii64(line_end, next_line_start), string_u8_litexpr(" "));
}

CUSTOM_COMMAND_SIG(snd_move_line_down) {
    View_ID view = get_active_view(app, Access_ReadWriteVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWriteVisible);

    if (!buffer_exists(app, buffer)) {
        return;
    }

    i64 line = get_line_number_from_pos(app, buffer, view_get_cursor_pos(app, view));
    move_line(app, buffer, line, Scan_Forward);
    move_down_textual(app);
}

CUSTOM_COMMAND_SIG(snd_move_up_textual)
CUSTOM_DOC("Moves up to the next line of actual text, regardless of line wrapping.")
{
    View_ID view = get_active_view(app, Access_ReadWriteVisible);
    i64 pos = view_get_cursor_pos(app, view);
    Buffer_Cursor cursor = view_compute_cursor(app, view, seek_pos(pos));
    i64 next_line = cursor.line - 1;
    view_set_cursor_and_preferred_x(app, view, seek_line_col(next_line, 1));
}

CUSTOM_COMMAND_SIG(snd_move_line_up) {
    View_ID view = get_active_view(app, Access_ReadWriteVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWriteVisible);

    if (!buffer_exists(app, buffer)) {
        return;
    }

    i64 line = get_line_number_from_pos(app, buffer, view_get_cursor_pos(app, view));
    move_line(app, buffer, line, Scan_Backward);
    snd_move_up_textual(app);
}

internal b32 character_is_newline(char c) {
    return c == '\r' || c == '\n';
}

internal i64 boundary_whitespace(Application_Links *app, Buffer_ID buffer, Side side, Scan_Direction direction, i64 pos) {
    return(boundary_predicate(app, buffer, side, direction, pos, &character_predicate_whitespace));
}

internal i64 snd_boundary_word(Application_Links* app, Buffer_ID buffer, Side side, Scan_Direction direction, i64 pos) {
    i64 result = 0;
    if (direction == Scan_Forward) {
        result = Min(buffer_seek_character_class_change_0_1(app, buffer, &character_predicate_alpha_numeric_underscore_utf8, direction, pos),
                     buffer_seek_character_class_change_1_0(app, buffer, &character_predicate_alpha_numeric_underscore_utf8, direction, pos));
        result = Min(result, buffer_seek_character_class_change_1_0(app, buffer, &character_predicate_whitespace, direction, pos));
    } else {
        result = Max(buffer_seek_character_class_change_0_1(app, buffer, &character_predicate_alpha_numeric_underscore_utf8, direction, pos),
                     buffer_seek_character_class_change_1_0(app, buffer, &character_predicate_alpha_numeric_underscore_utf8, direction, pos));
        result = Max(result, buffer_seek_character_class_change_1_0(app, buffer, &character_predicate_whitespace, direction, pos));
    }

    return result;
}

internal Snd_Motion_Result snd_execute_text_motion(Application_Links* app, Buffer_ID buffer, i64 start_pos_init, Scan_Direction dir, Snd_Text_Action associated_action, i32 motion_count, Snd_Text_Motion motion) {
    Snd_Motion_Result outer_result = { Ii64_neg_inf, start_pos_init };

    for (i32 motion_index = 0; motion_index < motion_count; motion_index++) {
        i64 start_pos = outer_result.seek_pos;

        Snd_Motion_Result inner_result = snd_null_motion(start_pos);

        switch (motion) {
            case SndTextMotion_Word: {
                // @TODO: Simplify this, figure out how it should _really_ work
                Scratch_Block scratch(app);

                i64 end_pos = start_pos;

                if (dir == Scan_Forward) {
                    if (line_is_valid_and_blank(app, buffer, get_line_number_from_pos(app, buffer, end_pos))) {
                        i64 next_line = get_line_number_from_pos(app, buffer, end_pos) + 1;
                        i64 next_line_start = get_pos_past_lead_whitespace_from_line_number(app, buffer, next_line);

                        end_pos = next_line_start;
                    } else {
                        end_pos = scan(app, push_boundary_list(scratch, snd_boundary_word, boundary_line), buffer, Scan_Forward, end_pos);

                        u8 c_at_cursor = buffer_get_char(app, buffer, end_pos);
                        if (!character_is_newline(c_at_cursor) && character_is_whitespace(c_at_cursor)) {
                            end_pos = scan(app, push_boundary_list(scratch, boundary_whitespace, boundary_line), buffer, Scan_Forward, end_pos);
                        }
                    }

                    if (associated_action && motion_index + 1 >= motion_count) {
                        // @Note: To mimic vim behaviour, we'll have to slice off one character from the end when using word motion for text actions
                        end_pos--;
                    }
                } else {
                    if (line_is_valid_and_blank(app, buffer, get_line_number_from_pos(app, buffer, end_pos))) {
                        i64 next_line = get_line_number_from_pos(app, buffer, end_pos) - 1;
                        i64 next_line_end = get_line_end_pos(app, buffer, next_line);

                        end_pos = next_line_end;
                    } else {
                        end_pos = scan(app, push_boundary_list(scratch, snd_boundary_word, boundary_line), buffer, Scan_Backward, end_pos);

                        u8 c_at_cursor = buffer_get_char(app, buffer, end_pos);
                        if (!character_is_newline(c_at_cursor) && character_is_whitespace(c_at_cursor)) {
                            end_pos = scan(app, push_boundary_list(scratch, boundary_whitespace, boundary_line), buffer, Scan_Backward, end_pos);
                        }
                    }
                }

                inner_result.seek_pos = end_pos;
                inner_result.range = Ii64(start_pos, end_pos);
            } break;

            case SndTextMotion_WordEnd: {
                Scratch_Block scratch(app);
                // @TODO: Use consistent word definition
                inner_result.seek_pos = scan(app, push_boundary_list(scratch, boundary_token), buffer, Scan_Forward, start_pos + 1) - 1; // @Unicode
                inner_result.range = Ii64(start_pos, inner_result.seek_pos);
            } break;

            case SndTextMotion_LineSide: {
                inner_result.range = Ii64(start_pos, get_line_side_pos_from_pos(app, buffer, start_pos, dir == Scan_Forward ? Side_Max : Side_Min));
                inner_result.seek_pos = (dir == Scan_Forward) ? inner_result.range.max : inner_result.range.min;
            } break;

            case SndTextMotion_ScopeVimStyle: {
                Token_Array token_array = get_token_array_from_buffer(app, buffer);
                if (token_array.count > 0) {
                    Token_Base_Kind opening_token = TokenBaseKind_EOF;
                    Range_i64 line_range = get_line_pos_range(app, buffer, get_line_number_from_pos(app, buffer, start_pos));
                    Token_Iterator_Array it = token_iterator_pos(0, &token_array, start_pos);

                    b32 loop = true;
                    while (loop && opening_token == TokenBaseKind_EOF) {
                        Token* token = token_it_read(&it);
                        if (range_contains_inclusive(line_range, token->pos)) {
                            switch (token->kind) {
                                case TokenBaseKind_ParentheticalOpen:
                                case TokenBaseKind_ScopeOpen:
                                case TokenBaseKind_ParentheticalClose:
                                case TokenBaseKind_ScopeClose: {
                                    opening_token = token->kind;
                                } break;

                                default: {
                                    loop = token_it_inc(&it);
                                } break;
                            }
                        } else {
                            break;
                        }
                    }

                    if (opening_token != TokenBaseKind_EOF) {
                        b32 scan_forward = (opening_token == TokenBaseKind_ScopeOpen || opening_token == TokenBaseKind_ParentheticalOpen);

                        Token_Base_Kind closing_token = TokenBaseKind_EOF;
                        if (opening_token == TokenBaseKind_ParentheticalOpen)  closing_token = TokenBaseKind_ParentheticalClose;
                        if (opening_token == TokenBaseKind_ParentheticalClose) closing_token = TokenBaseKind_ParentheticalOpen;
                        if (opening_token == TokenBaseKind_ScopeOpen)          closing_token = TokenBaseKind_ScopeClose;
                        if (opening_token == TokenBaseKind_ScopeClose)         closing_token = TokenBaseKind_ScopeOpen;

                        i32 scope_depth = 1;
                        for (;;) {
                            b32 tokens_left = scan_forward ? token_it_inc(&it) : token_it_dec(&it);
                            if (tokens_left) {
                                Token* token = token_it_read(&it);

                                if (token->kind == opening_token) {
                                    scope_depth++;
                                } else if (token->kind == closing_token) {
                                    scope_depth--;
                                }

                                if (scope_depth == 0) {
                                    i64 end_pos = token->pos;
                                    if (scan_forward) {
                                        end_pos += token->size - 1;
                                    }

                                    inner_result.seek_pos = end_pos;
                                    inner_result.range = Ii64(start_pos, end_pos);

                                    break;
                                }
                            } else {
                                break;
                            }
                        }
                    }
                }
            } break;

            case SndTextMotion_Scope4CoderStyle: {
                Find_Nest_Flag flags = FindNest_Scope;
                Range_i64 range = Ii64(start_pos);
                if (dir == Scan_Forward) {
                    if (find_nest_side(app, buffer, range.start + 1, flags, Scan_Forward, NestDelim_Open, &range) &&
                        find_nest_side(app, buffer, range.end, flags|FindNest_Balanced|FindNest_EndOfToken, Scan_Forward, NestDelim_Close, &range.end)
                    ) {
                        inner_result.range = range;
                        inner_result.seek_pos = range.min;
                    }
                } else {
                    if (find_nest_side(app, buffer, start_pos - 1, flags, Scan_Backward, NestDelim_Open, &range) &&
                        find_nest_side(app, buffer, range.end, flags|FindNest_Balanced|FindNest_EndOfToken, Scan_Forward, NestDelim_Close, &range.end)
                    ) {
                        inner_result.range = range;
                        inner_result.seek_pos = range.min;
                    }
                }
            } break;

            case SndTextMotion_SurroundingScope: {
                Range_i64 range = Ii64(start_pos);
                if (find_surrounding_nest(app, buffer, range.min, FindNest_Scope, &range)) {
                    for (;;) {
                        if (!find_surrounding_nest(app, buffer, range.min, FindNest_Scope, &range)) {
                            break;
                        }
                    }
                }
                inner_result.range = range;
                inner_result.seek_pos = range.min;
            } break;
        }

        outer_result = { range_union(outer_result.range, inner_result.range), inner_result.seek_pos };
    }

    return outer_result;
}

internal void snd_cut_range_inclusive(Application_Links* app, View_ID view, Buffer_ID buffer, Range_i64 range) {
    view_set_cursor(app, view, seek_pos(range.min));
    view_set_mark(app, view, seek_pos(range.max + 1));

    cut(app);
}

internal void snd_execute_text_action(Application_Links* app, View_ID view, Buffer_ID buffer, Snd_Text_Action action, Snd_Motion_Result mr) {
    switch (action) {
        case SndTextAction_Cut: {
            snd_cut_range_inclusive(app, view, buffer, mr.range);
            snd_end_history_group_for_buffer(app, buffer);
        } break;

        case SndTextAction_Change: {
            snd_cut_range_inclusive(app, view, buffer, mr.range);
            snd_exit_command_mode(app);
        } break;
    }
}

inline void snd_execute_text_command(Application_Links* app, View_ID view, Buffer_ID buffer, Snd_Text_Command command) {
    if (command.motion) {
        Snd_Motion_Result mr = snd_execute_text_motion(app, buffer, view_get_cursor_pos(app, view), command.motion_direction, command.action, command.motion_count, command.motion);
        u32 buffer_flags = buffer_get_access_flags(app, buffer);
        if (command.action && (buffer_flags & Access_Write)) {
            snd_most_recent_text_command = command;

            History_Group* history = snd_begin_history_group_for_buffer(app, buffer);
            if (!snd_most_recent_text_command.history_record_index) {
                snd_most_recent_text_command.history_record_index = history->first;
            }

            snd_execute_text_action(app, view, buffer, command.action, mr);
        } else {
            view_set_cursor_and_preferred_x(app, view, seek_pos(mr.seek_pos));
        }
    }
}

enum Snd_Command_Kind {
    SndCommandKind_None,

    SndCommandKind_Utiliy,
    SndCommandKind_Window,
    SndCommandKind_Text,
};

internal CUSTOM_COMMAND_SIG(snd_handle_chordal_input) {
    View_ID view = get_active_view(app, Access_ReadVisible);
    Buffer_ID buffer = view_get_buffer(app, view, Access_ReadVisible);

    if (!buffer_exists(app, buffer)) {
        return;
    }

    Snd_Command_Kind command_kind = SndCommandKind_None;

    Key_Code utility_kc = 0;

    Snd_Window_Action window_action = SndWindowAction_None;

    Snd_Text_Command text_command = {};
    text_command.motion_direction = Scan_Forward;

    User_Input in = get_current_input(app);

    b32 first_time_in_loop = true;
    for (;;) {
        if (first_time_in_loop) {
            first_time_in_loop = false;
        } else {
            in = get_next_input(app, (EventPropertyGroup_AnyKeyboardEvent & ~EventProperty_AnyKeyRelease), EventProperty_Escape|EventProperty_ViewActivation);
        }

        if (in.abort) {
            break;
        }

        b32 ctrl_down  = has_modifier(&in, KeyCode_Control);
        b32 shift_down = has_modifier(&in, KeyCode_Shift);
        Key_Code kc = in.event.key.code;

        if (in.event.kind == InputEventKind_KeyStroke) {
            //
            // Complete Actions
            //
            b32 hit_any = true;
            if (kc == KeyCode_G && shift_down) {
                // @Note: Inline command
                goto_end_of_file(app);
            } else {
                hit_any = false;
            } if (hit_any) break;

            //
            // Action Leaders
            //
            Snd_Text_Action entered_action = SndTextAction_None;
            if (kc == KeyCode_C) {
                entered_action = SndTextAction_Change;
            } else if (kc == KeyCode_D) {
                entered_action = SndTextAction_Cut;
            }

            if (entered_action) {
                if (!text_command.action) {
                    text_command.action = entered_action;
                    command_kind = SndCommandKind_Text;
                    continue;
                } else {
                    if (text_command.action == entered_action) {
                        // @Incomplete: Do the right behaviour
                        break;
                    } else {
                        break;
                    }
                }
            }

            if (!command_kind) {
                hit_any = true;
                if (kc == KeyCode_G || kc == KeyCode_Z) {
                    command_kind = SndCommandKind_Utiliy;
                    utility_kc = kc;
                } else if (kc == KeyCode_W && ctrl_down) {
                    if (command_kind == SndCommandKind_Window) {
                        window_action = SndWindowAction_Cycle;
                    }
                    command_kind = SndCommandKind_Window;
                } else {
                    hit_any = false;
                } if (hit_any) continue;
            }

            //
            // Action Tails
            //
            if (command_kind == SndCommandKind_Utiliy) {
                if (utility_kc == KeyCode_Z) {
                    if (kc == KeyCode_Z) {
                        center_view(app);
                        break;
                    }
                }

                if (utility_kc == KeyCode_G) {
                    if (kc == KeyCode_G) {
                        goto_beginning_of_file(app);
                        break;
                    }
                }
            }

            //
            // Motion Count
            //
            if (kc >= KeyCode_0 && kc <= KeyCode_9) {
                i32 count = cast(i32) (kc - KeyCode_0);
                if (!text_command.motion_count) {
                    if (kc != KeyCode_0) {
                        text_command.motion_count = count;
                        continue;
                    }
                } else {
                    text_command.motion_count = 10*text_command.motion_count + count;
                    continue;
                }
            }

            //
            // Action Tails / Motions
            //
            hit_any = true;
            if (kc == KeyCode_W) {
                text_command.motion = SndTextMotion_Word;
            } else if (kc == KeyCode_H) {
                window_action = SndWindowAction_Left;
            } else if (kc == KeyCode_J) {
                window_action = SndWindowAction_Down;
            } else if (kc == KeyCode_K) {
                window_action = SndWindowAction_Up;
            } else if (kc == KeyCode_L) {
                window_action = SndWindowAction_Right;
            } else if (kc == KeyCode_V) {
                window_action = SndWindowAction_VSplit;
            } else if (kc == KeyCode_S) {
                window_action = SndWindowAction_HSplit;
            } else if (kc == KeyCode_X) {
                window_action = SndWindowAction_Swap;
            } else if (kc == KeyCode_E) {
                text_command.motion = SndTextMotion_WordEnd;
            } else if (kc == KeyCode_B) {
                text_command.motion = SndTextMotion_Word;
                text_command.motion_direction = Scan_Backward;
            } else if (kc == KeyCode_0) {
                text_command.motion = SndTextMotion_LineSide;
                text_command.motion_direction = Scan_Backward;
            } else if (kc == KeyCode_4 && shift_down) {
                text_command.motion = SndTextMotion_LineSide;
            } else if (kc == KeyCode_5 && shift_down) {
                text_command.motion = SndTextMotion_ScopeVimStyle;
            } else if (kc == KeyCode_LeftBracket && ctrl_down && shift_down) {
                text_command.motion = SndTextMotion_SurroundingScope;
            } else if (kc == KeyCode_LeftBracket && shift_down) {
                text_command.motion = SndTextMotion_Scope4CoderStyle;
                text_command.motion_direction = Scan_Backward;
            } else if (kc == KeyCode_RightBracket && shift_down) {
                text_command.motion = SndTextMotion_Scope4CoderStyle;
            } else {
                hit_any = false;
            }

            if (hit_any) {
                break;
            }
        }
    }

    if (!text_command.motion_count) {
        text_command.motion_count = 1;
    }

    if (command_kind == SndCommandKind_Window) {
        Panel_ID panel = view_get_panel(app, view);
        switch (window_action) {
            case SndWindowAction_Cycle: { change_active_panel(app); } break;
            case SndWindowAction_HSplit: { panel_split(app, panel, Dimension_Y); } break;
            case SndWindowAction_VSplit: { panel_split(app, panel, Dimension_X); } break;
            case SndWindowAction_Swap: { swap_panels(app); } break;
        }
    } else {
        snd_execute_text_command(app, view, buffer, text_command);
    }
}

CUSTOM_COMMAND_SIG(snd_repeat_most_recent_command) {
    if (snd_text_command_is_valid(snd_most_recent_text_command)) {
        View_ID view = get_active_view(app, Access_ReadWriteVisible);
        Buffer_ID buffer = view_get_buffer(app, view, Access_ReadWriteVisible);

        if (!buffer_exists(app, buffer)) {
            return;
        }

        snd_execute_text_command(app, view, buffer, snd_most_recent_text_command);

        Record_Info record_info = buffer_history_get_record_info(app, buffer, snd_most_recent_text_command.history_record_index);
        if (record_info.single_string_forward.size > 0) {
            buffer_replace_range(app, buffer, Ii64(view_get_cursor_pos(app, view)), record_info.single_string_forward);
            snd_enter_command_mode(app);
        }
    }
}

function void snd_setup_mapping(Mapping *mapping, i64 global_id, i64 shared_id, i64 file_id, i64 code_id, i64 command_id) {
    MappingScope();
    SelectMapping(mapping);

    SelectMap(global_id);
    {
        BindCore(default_startup, CoreCode_Startup);
        BindCore(default_try_exit, CoreCode_TryExit);
        Bind(keyboard_macro_start_recording ,   KeyCode_U, KeyCode_Control);
        Bind(keyboard_macro_finish_recording,   KeyCode_U, KeyCode_Control, KeyCode_Shift);
        Bind(keyboard_macro_replay,             KeyCode_U, KeyCode_Alt);
        Bind(change_active_panel,               KeyCode_W, KeyCode_Control);
        Bind(change_active_panel_backwards,     KeyCode_Comma, KeyCode_Control, KeyCode_Shift);
        Bind(interactive_new,                   KeyCode_N, KeyCode_Control);
        Bind(interactive_open_or_new,           KeyCode_O, KeyCode_Control);
        Bind(open_in_other,                     KeyCode_O, KeyCode_Alt);
        Bind(interactive_kill_buffer,           KeyCode_K, KeyCode_Control);
        Bind(interactive_switch_buffer,         KeyCode_I, KeyCode_Control);
        Bind(project_go_to_root_directory,      KeyCode_H, KeyCode_Control);
        Bind(save_all_dirty_buffers,            KeyCode_S, KeyCode_Control, KeyCode_Shift);
        Bind(change_to_build_panel,             KeyCode_Period, KeyCode_Alt);
        Bind(close_build_panel,                 KeyCode_Comma, KeyCode_Alt);
        Bind(goto_next_jump,                    KeyCode_N, KeyCode_Control);
        Bind(goto_prev_jump,                    KeyCode_P, KeyCode_Control);
        Bind(build_in_build_panel,              KeyCode_M, KeyCode_Alt);
        Bind(goto_first_jump,                   KeyCode_M, KeyCode_Alt, KeyCode_Shift);
        Bind(toggle_filebar,                    KeyCode_B, KeyCode_Alt);
        Bind(execute_any_cli,                   KeyCode_Z, KeyCode_Alt);
        Bind(execute_previous_cli,              KeyCode_Z, KeyCode_Alt, KeyCode_Shift);
        Bind(command_lister,                    KeyCode_X, KeyCode_Alt);
        Bind(project_command_lister,            KeyCode_X, KeyCode_Alt, KeyCode_Shift);
        Bind(list_all_functions_current_buffer, KeyCode_I, KeyCode_Control, KeyCode_Shift);
        Bind(project_fkey_command,              KeyCode_F1);
        Bind(project_fkey_command,              KeyCode_F2);
        Bind(project_fkey_command,              KeyCode_F3);
        Bind(project_fkey_command,              KeyCode_F4);
        Bind(project_fkey_command,              KeyCode_F5);
        Bind(project_fkey_command,              KeyCode_F6);
        Bind(project_fkey_command,              KeyCode_F7);
        Bind(project_fkey_command,              KeyCode_F8);
        Bind(project_fkey_command,              KeyCode_F9);
        Bind(project_fkey_command,              KeyCode_F10);
        Bind(project_fkey_command,              KeyCode_F11);
        Bind(project_fkey_command,              KeyCode_F12);
        Bind(project_fkey_command,              KeyCode_F13);
        Bind(project_fkey_command,              KeyCode_F14);
        Bind(project_fkey_command,              KeyCode_F15);
        Bind(project_fkey_command,              KeyCode_F16);
        Bind(exit_4coder,                       KeyCode_F4, KeyCode_Alt);
        BindMouseWheel(mouse_wheel_scroll);
        BindMouseWheel(mouse_wheel_change_face_size, KeyCode_Control);
    }

    SelectMap(shared_id);
    {
        ParentMap(global_id);
        Bind(center_view,                                   KeyCode_E, KeyCode_Control);
        Bind(left_adjust_view,                              KeyCode_E, KeyCode_Control, KeyCode_Shift);
        Bind(search,                                        KeyCode_F, KeyCode_Control);
        Bind(list_all_locations,                            KeyCode_F, KeyCode_Control, KeyCode_Shift);
        Bind(list_all_substring_locations_case_insensitive, KeyCode_F, KeyCode_Alt);
        Bind(list_all_locations_of_selection,               KeyCode_G, KeyCode_Control, KeyCode_Shift);
        Bind(snippet_lister,                                KeyCode_J, KeyCode_Control);
        Bind(kill_buffer,                                   KeyCode_K, KeyCode_Control, KeyCode_Shift);
        Bind(duplicate_line,                                KeyCode_L, KeyCode_Control);
        Bind(cursor_mark_swap,                              KeyCode_M, KeyCode_Control);
        Bind(reopen,                                        KeyCode_O, KeyCode_Control, KeyCode_Shift);
        Bind(query_replace,                                 KeyCode_Q, KeyCode_Control);
        Bind(query_replace_identifier,                      KeyCode_Q, KeyCode_Control, KeyCode_Shift);
        Bind(query_replace_selection,                       KeyCode_Q, KeyCode_Alt);
        Bind(reverse_search,                                KeyCode_R, KeyCode_Control);
        Bind(save,                                          KeyCode_S, KeyCode_Control);
        Bind(save_all_dirty_buffers,                        KeyCode_S, KeyCode_Control, KeyCode_Shift);
        Bind(search_identifier,                             KeyCode_T, KeyCode_Control);
        Bind(list_all_locations_of_identifier,              KeyCode_T, KeyCode_Control, KeyCode_Shift);
        Bind(view_buffer_other_panel,                       KeyCode_1, KeyCode_Control);
        Bind(swap_panels,                                   KeyCode_2, KeyCode_Control);
    }

    SelectMap(file_id);
    {
        ParentMap(shared_id);
        BindTextInput(write_text_input);
        BindMouse(click_set_cursor_and_mark, MouseCode_Left);
        BindMouseRelease(click_set_cursor, MouseCode_Left);
        BindCore(click_set_cursor_and_mark, CoreCode_ClickActivateView);
        BindMouseMove(click_set_cursor_if_lbutton);
        Bind(delete_char,                                 KeyCode_Delete);
        Bind(backspace_char,                              KeyCode_Backspace);
        Bind(move_up,                                     KeyCode_Up);
        Bind(move_down,                                   KeyCode_Down);
        Bind(move_left,                                   KeyCode_Left);
        Bind(move_right,                                  KeyCode_Right);
        Bind(seek_end_of_line,                            KeyCode_End);
        Bind(seek_beginning_of_line,                      KeyCode_Home);
        Bind(page_up,                                     KeyCode_PageUp);
        Bind(page_down,                                   KeyCode_PageDown);
        Bind(goto_beginning_of_file,                      KeyCode_PageUp, KeyCode_Control);
        Bind(goto_end_of_file,                            KeyCode_PageDown, KeyCode_Control);
        Bind(move_up_to_blank_line_end,                   KeyCode_Up, KeyCode_Control);
        Bind(move_down_to_blank_line_end,                 KeyCode_Down, KeyCode_Control);
        Bind(move_left_whitespace_boundary,               KeyCode_Left, KeyCode_Control);
        Bind(move_right_whitespace_boundary,              KeyCode_Right, KeyCode_Control);
        Bind(move_line_up,                                KeyCode_Up, KeyCode_Alt);
        Bind(move_line_down,                              KeyCode_Down, KeyCode_Alt);
        Bind(backspace_alpha_numeric_boundary,            KeyCode_Backspace, KeyCode_Control);
        Bind(delete_alpha_numeric_boundary,               KeyCode_Delete, KeyCode_Control);
        Bind(snipe_backward_whitespace_or_token_boundary, KeyCode_Backspace, KeyCode_Alt);
        Bind(snipe_forward_whitespace_or_token_boundary,  KeyCode_Delete, KeyCode_Alt);
        Bind(set_mark,                                    KeyCode_Space, KeyCode_Control);
        Bind(replace_in_range,                            KeyCode_A, KeyCode_Control);
        Bind(copy,                                        KeyCode_C, KeyCode_Control);
        Bind(delete_range,                                KeyCode_D, KeyCode_Control);
        Bind(delete_line,                                 KeyCode_D, KeyCode_Control, KeyCode_Shift);
        Bind(goto_line,                                   KeyCode_G, KeyCode_Control);
        Bind(paste_and_indent,                            KeyCode_V, KeyCode_Control);
        Bind(paste_next_and_indent,                       KeyCode_V, KeyCode_Control, KeyCode_Shift);
        Bind(cut,                                         KeyCode_X, KeyCode_Control);
        Bind(redo,                                        KeyCode_Y, KeyCode_Control);
        Bind(undo,                                        KeyCode_Z, KeyCode_Control);
        Bind(if_read_only_goto_position,                  KeyCode_Return);
        Bind(if_read_only_goto_position_same_panel,       KeyCode_Return, KeyCode_Shift);
        Bind(view_jump_list_with_lister,                  KeyCode_Period, KeyCode_Control, KeyCode_Shift);
        Bind(snd_enter_command_mode,                      KeyCode_Escape);
    }

    SelectMap(code_id);
    {
        ParentMap(file_id);
        BindTextInput(write_text_and_auto_indent);
        Bind(move_left_alpha_numeric_boundary,                    KeyCode_Left, KeyCode_Control);
        Bind(move_right_alpha_numeric_boundary,                   KeyCode_Right, KeyCode_Control);
        Bind(move_left_alpha_numeric_or_camel_boundary,           KeyCode_Left, KeyCode_Alt);
        Bind(move_right_alpha_numeric_or_camel_boundary,          KeyCode_Right, KeyCode_Alt);
        Bind(comment_line_toggle,                                 KeyCode_Semicolon, KeyCode_Control);
        Bind(word_complete,                                       KeyCode_Tab);
        Bind(auto_indent_range,                                   KeyCode_Tab, KeyCode_Control);
        Bind(auto_indent_line_at_cursor,                          KeyCode_Tab, KeyCode_Shift);
        Bind(word_complete_drop_down,                             KeyCode_Tab, KeyCode_Shift, KeyCode_Control);
        Bind(write_block,                                         KeyCode_R, KeyCode_Alt);
        Bind(write_todo,                                          KeyCode_T, KeyCode_Alt);
        Bind(write_note,                                          KeyCode_Y, KeyCode_Alt);
        Bind(list_all_locations_of_type_definition,               KeyCode_D, KeyCode_Alt);
        Bind(list_all_locations_of_type_definition_of_identifier, KeyCode_T, KeyCode_Alt, KeyCode_Shift);
        Bind(open_long_braces,                                    KeyCode_LeftBracket, KeyCode_Control);
        Bind(open_long_braces_semicolon,                          KeyCode_LeftBracket, KeyCode_Control, KeyCode_Shift);
        Bind(open_long_braces_break,                              KeyCode_RightBracket, KeyCode_Control, KeyCode_Shift);
        Bind(select_surrounding_scope,                            KeyCode_LeftBracket, KeyCode_Alt);
        Bind(select_surrounding_scope_maximal,                    KeyCode_LeftBracket, KeyCode_Alt, KeyCode_Shift);
        Bind(select_prev_scope_absolute,                          KeyCode_RightBracket, KeyCode_Alt);
        Bind(select_prev_top_most_scope,                          KeyCode_RightBracket, KeyCode_Alt, KeyCode_Shift);
        Bind(select_next_scope_absolute,                          KeyCode_Quote, KeyCode_Alt);
        Bind(select_next_scope_after_current,                     KeyCode_Quote, KeyCode_Alt, KeyCode_Shift);
        Bind(place_in_scope,                                      KeyCode_ForwardSlash, KeyCode_Alt);
        Bind(delete_current_scope,                                KeyCode_Minus, KeyCode_Alt);
        Bind(if0_off,                                             KeyCode_I, KeyCode_Alt);
        Bind(open_file_in_quotes,                                 KeyCode_1, KeyCode_Alt);
        Bind(open_matching_file_cpp,                              KeyCode_2, KeyCode_Alt);
        Bind(write_zero_struct,                                   KeyCode_0, KeyCode_Control);
    }

    SelectMap(command_id);
    {
        ParentMap(shared_id);
        // @Note(snd): These are handled specially
        Bind(snd_handle_chordal_input,                      KeyCode_W);
        Bind(snd_handle_chordal_input,                      KeyCode_W, KeyCode_Shift);
        Bind(snd_handle_chordal_input,                      KeyCode_W, KeyCode_Control);
        Bind(snd_handle_chordal_input,                      KeyCode_E);
        Bind(snd_handle_chordal_input,                      KeyCode_B);
        Bind(snd_handle_chordal_input,                      KeyCode_B, KeyCode_Shift);
        Bind(snd_handle_chordal_input,                      KeyCode_D);
        Bind(snd_handle_chordal_input,                      KeyCode_C);
        Bind(snd_handle_chordal_input,                      KeyCode_0);
        Bind(snd_handle_chordal_input,                      KeyCode_4, KeyCode_Shift);
        Bind(snd_handle_chordal_input,                      KeyCode_5, KeyCode_Shift);
        Bind(snd_handle_chordal_input,                      KeyCode_LeftBracket, KeyCode_Shift);
        Bind(snd_handle_chordal_input,                      KeyCode_LeftBracket, KeyCode_Control, KeyCode_Shift);
        Bind(snd_handle_chordal_input,                      KeyCode_RightBracket, KeyCode_Shift);
        Bind(snd_handle_chordal_input,                      KeyCode_Z);
        Bind(snd_handle_chordal_input,                      KeyCode_G);
        Bind(snd_handle_chordal_input,                      KeyCode_G, KeyCode_Shift);

        Bind(move_up,                                       KeyCode_K);
        Bind(move_up_to_blank_line_end,                     KeyCode_K, KeyCode_Control);
        Bind(snd_move_line_up,                              KeyCode_K, KeyCode_Alt);
        Bind(move_down,                                     KeyCode_J);
        Bind(move_down_to_blank_line_end,                   KeyCode_J, KeyCode_Control);
        Bind(snd_move_line_down,                            KeyCode_J, KeyCode_Alt);
        Bind(snd_join_line,                                 KeyCode_J, KeyCode_Shift);
        Bind(move_left,                                     KeyCode_H);
        Bind(move_right,                                    KeyCode_L);
        Bind(snd_new_line_below,                            KeyCode_O);
        Bind(snd_new_line_above,                            KeyCode_O, KeyCode_Shift);
        Bind(interactive_open_or_new,                       KeyCode_O, KeyCode_Control);
        Bind(move_right,                                    KeyCode_L);
        Bind(page_up,                                       KeyCode_B, KeyCode_Control);
        Bind(page_down,                                     KeyCode_F, KeyCode_Control);
        Bind(snd_repeat_most_recent_command,                KeyCode_Period);
        Bind(set_mark,                                      KeyCode_V);
        Bind(snd_select_line,                               KeyCode_V, KeyCode_Shift);
        Bind(delete_char,                                   KeyCode_X);
        Bind(snd_inclusive_cut,                             KeyCode_D, KeyCode_Shift);
        Bind(snd_change,                                    KeyCode_C, KeyCode_Shift);
        Bind(copy,                                          KeyCode_Y);
        Bind(paste_and_indent,                              KeyCode_P);
        Bind(goto_next_jump,                                KeyCode_N, KeyCode_Control);
        Bind(goto_prev_jump,                                KeyCode_P, KeyCode_Control);
        Bind(search,                                        KeyCode_ForwardSlash);
        Bind(undo,                                          KeyCode_U);
        Bind(redo,                                          KeyCode_R, KeyCode_Control);
        Bind(command_lister,                                KeyCode_Semicolon, KeyCode_Shift);
        Bind(snd_exit_command_mode,                         KeyCode_I);
        Bind(snd_append,                                    KeyCode_A);
        Bind(snd_append_to_line_end,                        KeyCode_A, KeyCode_Shift);

#if 0
        Bind(keyboard_macro_start_recording ,   KeyCode_U, KeyCode_Control);
        Bind(keyboard_macro_finish_recording,   KeyCode_U, KeyCode_Control, KeyCode_Shift);
        Bind(keyboard_macro_replay,             KeyCode_U, KeyCode_Alt);
#endif
    }
}

void custom_layer_init(Application_Links *app) {
    Thread_Context *tctx = get_thread_context(app);

    // NOTE(allen): setup for default framework
    async_task_handler_init(app, &global_async_system);
    code_index_init();
    buffer_modified_set_init();
    Profile_Global_List *list = get_core_profile_list(app);
    ProfileThreadName(tctx, list, string_u8_litexpr("main"));
    initialize_managed_id_metadata(app);
    set_default_color_scheme(app);

    // NOTE(allen): default hooks and command maps
    set_all_default_hooks(app);

    set_custom_hook(app, HookID_RenderCaller, snd_render_caller);
    set_custom_hook(app, HookID_BeginBuffer, snd_begin_buffer);
    // set_custom_hook(app, HookID_ViewEventHandler, snd_view_input_handler);

    mapping_init(tctx, &framework_mapping);
    // setup_default_mapping(&framework_mapping, mapid_global, mapid_file, mapid_code);
    snd_setup_mapping(&framework_mapping, mapid_global, snd_mapid_shared, mapid_file, mapid_code, snd_mapid_command_mode);
}

#endif //FCODER_DEFAULT_BINDINGS

// BOTTOM

