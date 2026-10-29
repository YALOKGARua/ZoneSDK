#pragma warning(push)
#pragma warning(disable : 4100)
inline ImGuiIO& Stub_GetIO() { Hit(0, "GetIO"); return Io(); }
inline ImGuiStyle& Stub_GetStyle() { Hit(1, "GetStyle"); return Style(); }
inline const char* Stub_GetVersion() { Hit(2, "GetVersion"); return "1.92.5"; }
inline bool Stub_Begin(const char* name, bool* p_open, ImGuiWindowFlags flags) { Hit(3, "Begin"); return {}; }
inline void Stub_End() { Hit(4, "End");  }
inline bool Stub_BeginChild(const char* str_id, const ImVec2& size, ImGuiChildFlags child_flags, ImGuiWindowFlags window_flags) { Hit(5, "BeginChild"); return true; }
inline bool Stub_BeginChild2(ImGuiID id, const ImVec2& size, ImGuiChildFlags child_flags, ImGuiWindowFlags window_flags) { Hit(6, "BeginChild2"); return true; }
inline void Stub_EndChild() { Hit(7, "EndChild");  }
inline bool Stub_IsWindowAppearing() { Hit(8, "IsWindowAppearing"); return {}; }
inline bool Stub_IsWindowCollapsed() { Hit(9, "IsWindowCollapsed"); return {}; }
inline bool Stub_IsWindowFocused(ImGuiFocusedFlags flags) { Hit(10, "IsWindowFocused"); return {}; }
inline bool Stub_IsWindowHovered(ImGuiHoveredFlags flags) { Hit(11, "IsWindowHovered"); return {}; }
inline ImDrawList* Stub_GetWindowDrawList() { Hit(12, "GetWindowDrawList"); return List(); }
inline float Stub_GetWindowDpiScale() { Hit(13, "GetWindowDpiScale"); return {}; }
inline ImVec2 Stub_GetWindowPos() { Hit(14, "GetWindowPos"); return ImVec2(40.0f, 40.0f); }
inline ImVec2 Stub_GetWindowSize() { Hit(15, "GetWindowSize"); return ImVec2(640.0f, 480.0f); }
inline float Stub_GetWindowWidth() { Hit(16, "GetWindowWidth"); return 640.0f; }
inline float Stub_GetWindowHeight() { Hit(17, "GetWindowHeight"); return 480.0f; }
inline void Stub_SetNextWindowPos(const ImVec2& pos, ImGuiCond cond, const ImVec2& pivot) { Hit(18, "SetNextWindowPos");  }
inline void Stub_SetNextWindowSize(const ImVec2& size, ImGuiCond cond) { Hit(19, "SetNextWindowSize");  }
inline void Stub_SetNextWindowSizeConstraints(const ImVec2& size_min, const ImVec2& size_max, ImGuiSizeCallback custom_callback, void* custom_callback_data) { Hit(20, "SetNextWindowSizeConstraints");  }
inline void Stub_SetNextWindowContentSize(const ImVec2& size) { Hit(21, "SetNextWindowContentSize");  }
inline void Stub_SetNextWindowCollapsed(bool collapsed, ImGuiCond cond) { Hit(22, "SetNextWindowCollapsed");  }
inline void Stub_SetNextWindowFocus() { Hit(23, "SetNextWindowFocus");  }
inline void Stub_SetNextWindowScroll(const ImVec2& scroll) { Hit(24, "SetNextWindowScroll");  }
inline void Stub_SetNextWindowBgAlpha(float alpha) { Hit(25, "SetNextWindowBgAlpha");  }
inline void Stub_SetWindowPos(const ImVec2& pos, ImGuiCond cond) { Hit(26, "SetWindowPos");  }
inline void Stub_SetWindowSize(const ImVec2& size, ImGuiCond cond) { Hit(27, "SetWindowSize");  }
inline void Stub_SetWindowCollapsed(bool collapsed, ImGuiCond cond) { Hit(28, "SetWindowCollapsed");  }
inline void Stub_SetWindowFocus() { Hit(29, "SetWindowFocus");  }
inline void Stub_SetWindowPos2(const char* name, const ImVec2& pos, ImGuiCond cond) { Hit(30, "SetWindowPos2");  }
inline void Stub_SetWindowSize2(const char* name, const ImVec2& size, ImGuiCond cond) { Hit(31, "SetWindowSize2");  }
inline void Stub_SetWindowCollapsed2(const char* name, bool collapsed, ImGuiCond cond) { Hit(32, "SetWindowCollapsed2");  }
inline void Stub_SetWindowFocus2(const char* name) { Hit(33, "SetWindowFocus2");  }
inline float Stub_GetScrollX() { Hit(34, "GetScrollX"); return {}; }
inline float Stub_GetScrollY() { Hit(35, "GetScrollY"); return {}; }
inline void Stub_SetScrollX(float scroll_x) { Hit(36, "SetScrollX");  }
inline void Stub_SetScrollY(float scroll_y) { Hit(37, "SetScrollY");  }
inline float Stub_GetScrollMaxX() { Hit(38, "GetScrollMaxX"); return {}; }
inline float Stub_GetScrollMaxY() { Hit(39, "GetScrollMaxY"); return {}; }
inline void Stub_SetScrollHereX(float center_x_ratio) { Hit(40, "SetScrollHereX");  }
inline void Stub_SetScrollHereY(float center_y_ratio) { Hit(41, "SetScrollHereY");  }
inline void Stub_SetScrollFromPosX(float local_x, float center_x_ratio) { Hit(42, "SetScrollFromPosX");  }
inline void Stub_SetScrollFromPosY(float local_y, float center_y_ratio) { Hit(43, "SetScrollFromPosY");  }
inline void Stub_PushFont(ImFont* font, float font_size_base_unscaled) { Hit(44, "PushFont");  }
inline void Stub_PopFont() { Hit(45, "PopFont");  }
inline ImFont* Stub_GetFont() { Hit(46, "GetFont"); return Font(); }
inline float Stub_GetFontSize() { Hit(47, "GetFontSize"); return 13.0f; }
inline ImFontBaked* Stub_GetFontBaked() { Hit(48, "GetFontBaked"); return {}; }
inline void Stub_PushStyleColor(ImGuiCol idx, ImU32 col) { Hit(49, "PushStyleColor");  }
inline void Stub_PushStyleColor2(ImGuiCol idx, const ImVec4& col) { Hit(50, "PushStyleColor2");  }
inline void Stub_PopStyleColor(int count) { Hit(51, "PopStyleColor");  }
inline void Stub_PushStyleVar(ImGuiStyleVar idx, float val) { Hit(52, "PushStyleVar");  }
inline void Stub_PushStyleVar2(ImGuiStyleVar idx, const ImVec2& val) { Hit(53, "PushStyleVar2");  }
inline void Stub_PushStyleVarX(ImGuiStyleVar idx, float val_x) { Hit(54, "PushStyleVarX");  }
inline void Stub_PushStyleVarY(ImGuiStyleVar idx, float val_y) { Hit(55, "PushStyleVarY");  }
inline void Stub_PopStyleVar(int count) { Hit(56, "PopStyleVar");  }
inline void Stub_PushItemFlag(ImGuiItemFlags option, bool enabled) { Hit(57, "PushItemFlag");  }
inline void Stub_PopItemFlag() { Hit(58, "PopItemFlag");  }
inline void Stub_PushItemWidth(float item_width) { Hit(59, "PushItemWidth");  }
inline void Stub_PopItemWidth() { Hit(60, "PopItemWidth");  }
inline void Stub_SetNextItemWidth(float item_width) { Hit(61, "SetNextItemWidth");  }
inline float Stub_CalcItemWidth() { Hit(62, "CalcItemWidth"); return {}; }
inline void Stub_PushTextWrapPos(float wrap_local_pos_x) { Hit(63, "PushTextWrapPos");  }
inline void Stub_PopTextWrapPos() { Hit(64, "PopTextWrapPos");  }
inline ImVec2 Stub_GetFontTexUvWhitePixel() { Hit(65, "GetFontTexUvWhitePixel"); return {}; }
inline ImU32 Stub_GetColorU32(ImGuiCol idx, float alpha_mul) { Hit(66, "GetColorU32"); return 0xFFFFFFFFu; }
inline ImU32 Stub_GetColorU322(const ImVec4& col) { Hit(67, "GetColorU322"); return 0xFFFFFFFFu; }
inline ImU32 Stub_GetColorU323(ImU32 col, float alpha_mul) { Hit(68, "GetColorU323"); return col; }
inline const ImVec4& Stub_GetStyleColorVec4(ImGuiCol idx) { Hit(69, "GetStyleColorVec4"); return Store<ImVec4>(); }
inline ImVec2 Stub_GetCursorScreenPos() { Hit(70, "GetCursorScreenPos"); return ImVec2(100.0f, 100.0f); }
inline void Stub_SetCursorScreenPos(const ImVec2& pos) { Hit(71, "SetCursorScreenPos");  }
inline ImVec2 Stub_GetContentRegionAvail() { Hit(72, "GetContentRegionAvail"); return ImVec2(600.0f, 400.0f); }
inline ImVec2 Stub_GetCursorPos() { Hit(73, "GetCursorPos"); return ImVec2(8.0f, 8.0f); }
inline float Stub_GetCursorPosX() { Hit(74, "GetCursorPosX"); return {}; }
inline float Stub_GetCursorPosY() { Hit(75, "GetCursorPosY"); return {}; }
inline void Stub_SetCursorPos(const ImVec2& local_pos) { Hit(76, "SetCursorPos");  }
inline void Stub_SetCursorPosX(float local_x) { Hit(77, "SetCursorPosX");  }
inline void Stub_SetCursorPosY(float local_y) { Hit(78, "SetCursorPosY");  }
inline ImVec2 Stub_GetCursorStartPos() { Hit(79, "GetCursorStartPos"); return {}; }
inline void Stub_Separator() { Hit(80, "Separator");  }
inline void Stub_SameLine(float offset_from_start_x, float spacing) { Hit(81, "SameLine");  }
inline void Stub_NewLine() { Hit(82, "NewLine");  }
inline void Stub_Spacing() { Hit(83, "Spacing");  }
inline void Stub_Dummy(const ImVec2& size) { Hit(84, "Dummy");  }
inline void Stub_Indent(float indent_w) { Hit(85, "Indent");  }
inline void Stub_Unindent(float indent_w) { Hit(86, "Unindent");  }
inline void Stub_BeginGroup() { Hit(87, "BeginGroup");  }
inline void Stub_EndGroup() { Hit(88, "EndGroup");  }
inline void Stub_AlignTextToFramePadding() { Hit(89, "AlignTextToFramePadding");  }
inline float Stub_GetTextLineHeight() { Hit(90, "GetTextLineHeight"); return {}; }
inline float Stub_GetTextLineHeightWithSpacing() { Hit(91, "GetTextLineHeightWithSpacing"); return {}; }
inline float Stub_GetFrameHeight() { Hit(92, "GetFrameHeight"); return {}; }
inline float Stub_GetFrameHeightWithSpacing() { Hit(93, "GetFrameHeightWithSpacing"); return {}; }
inline void Stub_PushID(const char* str_id) { Hit(94, "PushID");  }
inline void Stub_PushID2(const char* str_id_begin, const char* str_id_end) { Hit(95, "PushID2");  }
inline void Stub_PushID3(const void* ptr_id) { Hit(96, "PushID3");  }
inline void Stub_PushID4(int int_id) { Hit(97, "PushID4");  }
inline void Stub_PopID() { Hit(98, "PopID");  }
inline ImGuiID Stub_GetID(const char* str_id) { Hit(99, "GetID"); return {}; }
inline ImGuiID Stub_GetID2(const char* str_id_begin, const char* str_id_end) { Hit(100, "GetID2"); return {}; }
inline ImGuiID Stub_GetID3(const void* ptr_id) { Hit(101, "GetID3"); return {}; }
inline ImGuiID Stub_GetID4(int int_id) { Hit(102, "GetID4"); return {}; }
inline void Stub_TextUnformatted(const char* text, const char* text_end) { Hit(103, "TextUnformatted");  }
inline void Stub_TextV(const char* fmt, va_list args) { Hit(104, "TextV");  }
inline void Stub_TextColoredV(const ImVec4& col, const char* fmt, va_list args) { Hit(105, "TextColoredV");  }
inline void Stub_TextDisabledV(const char* fmt, va_list args) { Hit(106, "TextDisabledV");  }
inline void Stub_TextWrappedV(const char* fmt, va_list args) { Hit(107, "TextWrappedV");  }
inline void Stub_LabelTextV(const char* label, const char* fmt, va_list args) { Hit(108, "LabelTextV");  }
inline void Stub_BulletTextV(const char* fmt, va_list args) { Hit(109, "BulletTextV");  }
inline void Stub_SeparatorText(const char* label) { Hit(110, "SeparatorText");  }
inline bool Stub_Button(const char* label, const ImVec2& size) { Hit(111, "Button"); return {}; }
inline bool Stub_SmallButton(const char* label) { Hit(112, "SmallButton"); return {}; }
inline bool Stub_InvisibleButton(const char* str_id, const ImVec2& size, ImGuiButtonFlags flags) { Hit(113, "InvisibleButton"); return {}; }
inline bool Stub_ArrowButton(const char* str_id, ImGuiDir dir) { Hit(114, "ArrowButton"); return {}; }
inline bool Stub_Checkbox(const char* label, bool* v) { Hit(115, "Checkbox"); return {}; }
inline bool Stub_CheckboxFlags(const char* label, int* flags, int flags_value) { Hit(116, "CheckboxFlags"); return {}; }
inline bool Stub_CheckboxFlags2(const char* label, unsigned int* flags, unsigned int flags_value) { Hit(117, "CheckboxFlags2"); return {}; }
inline bool Stub_RadioButton(const char* label, bool active) { Hit(118, "RadioButton"); return {}; }
inline bool Stub_RadioButton2(const char* label, int* v, int v_button) { Hit(119, "RadioButton2"); return {}; }
inline void Stub_ProgressBar(float fraction, const ImVec2& size_arg, const char* overlay) { Hit(120, "ProgressBar");  }
inline void Stub_Bullet() { Hit(121, "Bullet");  }
inline bool Stub_TextLink(const char* label) { Hit(122, "TextLink"); return {}; }
inline bool Stub_TextLinkOpenURL(const char* label, const char* url) { Hit(123, "TextLinkOpenURL"); return {}; }
inline void Stub_Image(ImTextureRef tex_ref, const ImVec2& image_size, const ImVec2& uv0, const ImVec2& uv1) { Hit(124, "Image");  }
inline void Stub_ImageWithBg(ImTextureRef tex_ref, const ImVec2& image_size, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& bg_col, const ImVec4& tint_col) { Hit(125, "ImageWithBg");  }
inline bool Stub_ImageButton(const char* str_id, ImTextureRef tex_ref, const ImVec2& image_size, const ImVec2& uv0, const ImVec2& uv1, const ImVec4& bg_col, const ImVec4& tint_col) { Hit(126, "ImageButton"); return {}; }
inline bool Stub_BeginCombo(const char* label, const char* preview_value, ImGuiComboFlags flags) { Hit(127, "BeginCombo"); return {}; }
inline void Stub_EndCombo() { Hit(128, "EndCombo");  }
inline bool Stub_Combo(const char* label, int* current_item, const char* const items[], int items_count, int popup_max_height_in_items) { Hit(129, "Combo"); return {}; }
inline bool Stub_Combo2(const char* label, int* current_item, const char* items_separated_by_zeros, int popup_max_height_in_items) { Hit(130, "Combo2"); return {}; }
inline bool Stub_Combo3(const char* label, int* current_item, const char*(*getter)(void* user_data, int idx), void* user_data, int items_count, int popup_max_height_in_items) { Hit(131, "Combo3"); return {}; }
inline bool Stub_DragFloat(const char* label, float* v, float v_speed, float v_min, float v_max, const char* format, ImGuiSliderFlags flags) { Hit(132, "DragFloat"); return {}; }
inline bool Stub_DragFloat2(const char* label, float v[2], float v_speed, float v_min, float v_max, const char* format, ImGuiSliderFlags flags) { Hit(133, "DragFloat2"); return {}; }
inline bool Stub_DragFloat3(const char* label, float v[3], float v_speed, float v_min, float v_max, const char* format, ImGuiSliderFlags flags) { Hit(134, "DragFloat3"); return {}; }
inline bool Stub_DragFloat4(const char* label, float v[4], float v_speed, float v_min, float v_max, const char* format, ImGuiSliderFlags flags) { Hit(135, "DragFloat4"); return {}; }
inline bool Stub_DragFloatRange2(const char* label, float* v_current_min, float* v_current_max, float v_speed, float v_min, float v_max, const char* format, const char* format_max, ImGuiSliderFlags flags) { Hit(136, "DragFloatRange2"); return {}; }
inline bool Stub_DragInt(const char* label, int* v, float v_speed, int v_min, int v_max, const char* format, ImGuiSliderFlags flags) { Hit(137, "DragInt"); return {}; }
inline bool Stub_DragInt2(const char* label, int v[2], float v_speed, int v_min, int v_max, const char* format, ImGuiSliderFlags flags) { Hit(138, "DragInt2"); return {}; }
inline bool Stub_DragInt3(const char* label, int v[3], float v_speed, int v_min, int v_max, const char* format, ImGuiSliderFlags flags) { Hit(139, "DragInt3"); return {}; }
inline bool Stub_DragInt4(const char* label, int v[4], float v_speed, int v_min, int v_max, const char* format, ImGuiSliderFlags flags) { Hit(140, "DragInt4"); return {}; }
inline bool Stub_DragIntRange2(const char* label, int* v_current_min, int* v_current_max, float v_speed, int v_min, int v_max, const char* format, const char* format_max, ImGuiSliderFlags flags) { Hit(141, "DragIntRange2"); return {}; }
inline bool Stub_DragScalar(const char* label, ImGuiDataType data_type, void* p_data, float v_speed, const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags) { Hit(142, "DragScalar"); return {}; }
inline bool Stub_DragScalarN(const char* label, ImGuiDataType data_type, void* p_data, int components, float v_speed, const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags) { Hit(143, "DragScalarN"); return {}; }
inline bool Stub_SliderFloat(const char* label, float* v, float v_min, float v_max, const char* format, ImGuiSliderFlags flags) { Hit(144, "SliderFloat"); return {}; }
inline bool Stub_SliderFloat2(const char* label, float v[2], float v_min, float v_max, const char* format, ImGuiSliderFlags flags) { Hit(145, "SliderFloat2"); return {}; }
inline bool Stub_SliderFloat3(const char* label, float v[3], float v_min, float v_max, const char* format, ImGuiSliderFlags flags) { Hit(146, "SliderFloat3"); return {}; }
inline bool Stub_SliderFloat4(const char* label, float v[4], float v_min, float v_max, const char* format, ImGuiSliderFlags flags) { Hit(147, "SliderFloat4"); return {}; }
inline bool Stub_SliderAngle(const char* label, float* v_rad, float v_degrees_min, float v_degrees_max, const char* format, ImGuiSliderFlags flags) { Hit(148, "SliderAngle"); return {}; }
inline bool Stub_SliderInt(const char* label, int* v, int v_min, int v_max, const char* format, ImGuiSliderFlags flags) { Hit(149, "SliderInt"); return {}; }
inline bool Stub_SliderInt2(const char* label, int v[2], int v_min, int v_max, const char* format, ImGuiSliderFlags flags) { Hit(150, "SliderInt2"); return {}; }
inline bool Stub_SliderInt3(const char* label, int v[3], int v_min, int v_max, const char* format, ImGuiSliderFlags flags) { Hit(151, "SliderInt3"); return {}; }
inline bool Stub_SliderInt4(const char* label, int v[4], int v_min, int v_max, const char* format, ImGuiSliderFlags flags) { Hit(152, "SliderInt4"); return {}; }
inline bool Stub_SliderScalar(const char* label, ImGuiDataType data_type, void* p_data, const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags) { Hit(153, "SliderScalar"); return {}; }
inline bool Stub_SliderScalarN(const char* label, ImGuiDataType data_type, void* p_data, int components, const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags) { Hit(154, "SliderScalarN"); return {}; }
inline bool Stub_VSliderFloat(const char* label, const ImVec2& size, float* v, float v_min, float v_max, const char* format, ImGuiSliderFlags flags) { Hit(155, "VSliderFloat"); return {}; }
inline bool Stub_VSliderInt(const char* label, const ImVec2& size, int* v, int v_min, int v_max, const char* format, ImGuiSliderFlags flags) { Hit(156, "VSliderInt"); return {}; }
inline bool Stub_VSliderScalar(const char* label, const ImVec2& size, ImGuiDataType data_type, void* p_data, const void* p_min, const void* p_max, const char* format, ImGuiSliderFlags flags) { Hit(157, "VSliderScalar"); return {}; }
inline bool Stub_InputText(const char* label, char* buf, size_t buf_size, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data) { Hit(158, "InputText"); return {}; }
inline bool Stub_InputTextMultiline(const char* label, char* buf, size_t buf_size, const ImVec2& size, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data) { Hit(159, "InputTextMultiline"); return {}; }
inline bool Stub_InputTextWithHint(const char* label, const char* hint, char* buf, size_t buf_size, ImGuiInputTextFlags flags, ImGuiInputTextCallback callback, void* user_data) { Hit(160, "InputTextWithHint"); return {}; }
inline bool Stub_InputFloat(const char* label, float* v, float step, float step_fast, const char* format, ImGuiInputTextFlags flags) { Hit(161, "InputFloat"); return {}; }
inline bool Stub_InputFloat2(const char* label, float v[2], const char* format, ImGuiInputTextFlags flags) { Hit(162, "InputFloat2"); return {}; }
inline bool Stub_InputFloat3(const char* label, float v[3], const char* format, ImGuiInputTextFlags flags) { Hit(163, "InputFloat3"); return {}; }
inline bool Stub_InputFloat4(const char* label, float v[4], const char* format, ImGuiInputTextFlags flags) { Hit(164, "InputFloat4"); return {}; }
inline bool Stub_InputInt(const char* label, int* v, int step, int step_fast, ImGuiInputTextFlags flags) { Hit(165, "InputInt"); return {}; }
inline bool Stub_InputInt2(const char* label, int v[2], ImGuiInputTextFlags flags) { Hit(166, "InputInt2"); return {}; }
inline bool Stub_InputInt3(const char* label, int v[3], ImGuiInputTextFlags flags) { Hit(167, "InputInt3"); return {}; }
inline bool Stub_InputInt4(const char* label, int v[4], ImGuiInputTextFlags flags) { Hit(168, "InputInt4"); return {}; }
inline bool Stub_InputDouble(const char* label, double* v, double step, double step_fast, const char* format, ImGuiInputTextFlags flags) { Hit(169, "InputDouble"); return {}; }
inline bool Stub_InputScalar(const char* label, ImGuiDataType data_type, void* p_data, const void* p_step, const void* p_step_fast, const char* format, ImGuiInputTextFlags flags) { Hit(170, "InputScalar"); return {}; }
inline bool Stub_InputScalarN(const char* label, ImGuiDataType data_type, void* p_data, int components, const void* p_step, const void* p_step_fast, const char* format, ImGuiInputTextFlags flags) { Hit(171, "InputScalarN"); return {}; }
inline bool Stub_ColorEdit3(const char* label, float col[3], ImGuiColorEditFlags flags) { Hit(172, "ColorEdit3"); return {}; }
inline bool Stub_ColorEdit4(const char* label, float col[4], ImGuiColorEditFlags flags) { Hit(173, "ColorEdit4"); return {}; }
inline bool Stub_ColorPicker3(const char* label, float col[3], ImGuiColorEditFlags flags) { Hit(174, "ColorPicker3"); return {}; }
inline bool Stub_ColorPicker4(const char* label, float col[4], ImGuiColorEditFlags flags, const float* ref_col) { Hit(175, "ColorPicker4"); return {}; }
inline bool Stub_ColorButton(const char* desc_id, const ImVec4& col, ImGuiColorEditFlags flags, const ImVec2& size) { Hit(176, "ColorButton"); return {}; }
inline void Stub_SetColorEditOptions(ImGuiColorEditFlags flags) { Hit(177, "SetColorEditOptions");  }
inline bool Stub_TreeNode(const char* label) { Hit(178, "TreeNode"); return true; }
inline bool Stub_TreeNodeV(const char* str_id, const char* fmt, va_list args) { Hit(179, "TreeNodeV"); return true; }
inline bool Stub_TreeNodeV2(const void* ptr_id, const char* fmt, va_list args) { Hit(180, "TreeNodeV2"); return true; }
inline bool Stub_TreeNodeEx(const char* label, ImGuiTreeNodeFlags flags) { Hit(181, "TreeNodeEx"); return true; }
inline bool Stub_TreeNodeExV(const char* str_id, ImGuiTreeNodeFlags flags, const char* fmt, va_list args) { Hit(182, "TreeNodeExV"); return true; }
inline bool Stub_TreeNodeExV2(const void* ptr_id, ImGuiTreeNodeFlags flags, const char* fmt, va_list args) { Hit(183, "TreeNodeExV2"); return true; }
inline void Stub_TreePush(const char* str_id) { Hit(184, "TreePush");  }
inline void Stub_TreePush2(const void* ptr_id) { Hit(185, "TreePush2");  }
inline void Stub_TreePop() { Hit(186, "TreePop");  }
inline float Stub_GetTreeNodeToLabelSpacing() { Hit(187, "GetTreeNodeToLabelSpacing"); return {}; }
inline bool Stub_CollapsingHeader(const char* label, ImGuiTreeNodeFlags flags) { Hit(188, "CollapsingHeader"); return true; }
inline bool Stub_CollapsingHeader2(const char* label, bool* p_visible, ImGuiTreeNodeFlags flags) { Hit(189, "CollapsingHeader2"); return true; }
inline void Stub_SetNextItemOpen(bool is_open, ImGuiCond cond) { Hit(190, "SetNextItemOpen");  }
inline void Stub_SetNextItemStorageID(ImGuiID storage_id) { Hit(191, "SetNextItemStorageID");  }
inline bool Stub_Selectable(const char* label, bool selected, ImGuiSelectableFlags flags, const ImVec2& size) { Hit(192, "Selectable"); return {}; }
inline bool Stub_Selectable2(const char* label, bool* p_selected, ImGuiSelectableFlags flags, const ImVec2& size) { Hit(193, "Selectable2"); return {}; }
inline ImGuiMultiSelectIO* Stub_BeginMultiSelect(ImGuiMultiSelectFlags flags, int selection_size, int items_count) { Hit(194, "BeginMultiSelect"); return {}; }
inline ImGuiMultiSelectIO* Stub_EndMultiSelect() { Hit(195, "EndMultiSelect"); return {}; }
inline void Stub_SetNextItemSelectionUserData(ImGuiSelectionUserData selection_user_data) { Hit(196, "SetNextItemSelectionUserData");  }
inline bool Stub_IsItemToggledSelection() { Hit(197, "IsItemToggledSelection"); return {}; }
inline bool Stub_BeginListBox(const char* label, const ImVec2& size) { Hit(198, "BeginListBox"); return {}; }
inline void Stub_EndListBox() { Hit(199, "EndListBox");  }
inline bool Stub_ListBox(const char* label, int* current_item, const char* const items[], int items_count, int height_in_items) { Hit(200, "ListBox"); return {}; }
inline bool Stub_ListBox2(const char* label, int* current_item, const char*(*getter)(void* user_data, int idx), void* user_data, int items_count, int height_in_items) { Hit(201, "ListBox2"); return {}; }
inline void Stub_PlotLines(const char* label, const float* values, int values_count, int values_offset, const char* overlay_text, float scale_min, float scale_max, ImVec2 graph_size, int stride) { Hit(202, "PlotLines");  }
inline void Stub_PlotLines2(const char* label, float(*values_getter)(void* data, int idx), void* data, int values_count, int values_offset, const char* overlay_text, float scale_min, float scale_max, ImVec2 graph_size) { Hit(203, "PlotLines2");  }
inline void Stub_PlotHistogram(const char* label, const float* values, int values_count, int values_offset, const char* overlay_text, float scale_min, float scale_max, ImVec2 graph_size, int stride) { Hit(204, "PlotHistogram");  }
inline void Stub_PlotHistogram2(const char* label, float(*values_getter)(void* data, int idx), void* data, int values_count, int values_offset, const char* overlay_text, float scale_min, float scale_max, ImVec2 graph_size) { Hit(205, "PlotHistogram2");  }
inline void Stub_Value(const char* prefix, bool b) { Hit(206, "Value");  }
inline void Stub_Value2(const char* prefix, int v) { Hit(207, "Value2");  }
inline void Stub_Value3(const char* prefix, unsigned int v) { Hit(208, "Value3");  }
inline void Stub_Value4(const char* prefix, float v, const char* float_format) { Hit(209, "Value4");  }
inline bool Stub_BeginMenuBar() { Hit(210, "BeginMenuBar"); return {}; }
inline void Stub_EndMenuBar() { Hit(211, "EndMenuBar");  }
inline bool Stub_BeginMainMenuBar() { Hit(212, "BeginMainMenuBar"); return {}; }
inline void Stub_EndMainMenuBar() { Hit(213, "EndMainMenuBar");  }
inline bool Stub_BeginMenu(const char* label, bool enabled) { Hit(214, "BeginMenu"); return {}; }
inline void Stub_EndMenu() { Hit(215, "EndMenu");  }
inline bool Stub_MenuItem(const char* label, const char* shortcut, bool selected, bool enabled) { Hit(216, "MenuItem"); return {}; }
inline bool Stub_MenuItem2(const char* label, const char* shortcut, bool* p_selected, bool enabled) { Hit(217, "MenuItem2"); return {}; }
inline bool Stub_BeginTooltip() { Hit(218, "BeginTooltip"); return {}; }
inline void Stub_EndTooltip() { Hit(219, "EndTooltip");  }
inline void Stub_SetTooltipV(const char* fmt, va_list args) { Hit(220, "SetTooltipV");  }
inline bool Stub_BeginItemTooltip() { Hit(221, "BeginItemTooltip"); return {}; }
inline void Stub_SetItemTooltipV(const char* fmt, va_list args) { Hit(222, "SetItemTooltipV");  }
inline bool Stub_BeginPopup(const char* str_id, ImGuiWindowFlags flags) { Hit(223, "BeginPopup"); return {}; }
inline bool Stub_BeginPopupModal(const char* name, bool* p_open, ImGuiWindowFlags flags) { Hit(224, "BeginPopupModal"); return {}; }
inline void Stub_EndPopup() { Hit(225, "EndPopup");  }
inline void Stub_OpenPopup(const char* str_id, ImGuiPopupFlags popup_flags) { Hit(226, "OpenPopup");  }
inline void Stub_OpenPopup2(ImGuiID id, ImGuiPopupFlags popup_flags) { Hit(227, "OpenPopup2");  }
inline void Stub_OpenPopupOnItemClick(const char* str_id, ImGuiPopupFlags popup_flags) { Hit(228, "OpenPopupOnItemClick");  }
inline void Stub_CloseCurrentPopup() { Hit(229, "CloseCurrentPopup");  }
inline bool Stub_BeginPopupContextItem(const char* str_id, ImGuiPopupFlags popup_flags) { Hit(230, "BeginPopupContextItem"); return {}; }
inline bool Stub_BeginPopupContextWindow(const char* str_id, ImGuiPopupFlags popup_flags) { Hit(231, "BeginPopupContextWindow"); return {}; }
inline bool Stub_BeginPopupContextVoid(const char* str_id, ImGuiPopupFlags popup_flags) { Hit(232, "BeginPopupContextVoid"); return {}; }
inline bool Stub_IsPopupOpen(const char* str_id, ImGuiPopupFlags flags) { Hit(233, "IsPopupOpen"); return {}; }
inline bool Stub_BeginTable(const char* str_id, int columns, ImGuiTableFlags flags, const ImVec2& outer_size, float inner_width) { Hit(234, "BeginTable"); return true; }
inline void Stub_EndTable() { Hit(235, "EndTable");  }
inline void Stub_TableNextRow(ImGuiTableRowFlags row_flags, float min_row_height) { Hit(236, "TableNextRow");  }
inline bool Stub_TableNextColumn() { Hit(237, "TableNextColumn"); return {}; }
inline bool Stub_TableSetColumnIndex(int column_n) { Hit(238, "TableSetColumnIndex"); return {}; }
inline void Stub_TableSetupColumn(const char* label, ImGuiTableColumnFlags flags, float init_width_or_weight, ImGuiID user_id) { Hit(239, "TableSetupColumn");  }
inline void Stub_TableSetupScrollFreeze(int cols, int rows) { Hit(240, "TableSetupScrollFreeze");  }
inline void Stub_TableHeader(const char* label) { Hit(241, "TableHeader");  }
inline void Stub_TableHeadersRow() { Hit(242, "TableHeadersRow");  }
inline void Stub_TableAngledHeadersRow() { Hit(243, "TableAngledHeadersRow");  }
inline ImGuiTableSortSpecs* Stub_TableGetSortSpecs() { Hit(244, "TableGetSortSpecs"); return {}; }
inline int Stub_TableGetColumnCount() { Hit(245, "TableGetColumnCount"); return {}; }
inline int Stub_TableGetColumnIndex() { Hit(246, "TableGetColumnIndex"); return {}; }
inline int Stub_TableGetRowIndex() { Hit(247, "TableGetRowIndex"); return {}; }
inline const char* Stub_TableGetColumnName(int column_n) { Hit(248, "TableGetColumnName"); return {}; }
inline ImGuiTableColumnFlags Stub_TableGetColumnFlags(int column_n) { Hit(249, "TableGetColumnFlags"); return {}; }
inline void Stub_TableSetColumnEnabled(int column_n, bool v) { Hit(250, "TableSetColumnEnabled");  }
inline int Stub_TableGetHoveredColumn() { Hit(251, "TableGetHoveredColumn"); return {}; }
inline void Stub_TableSetBgColor(ImGuiTableBgTarget target, ImU32 color, int column_n) { Hit(252, "TableSetBgColor");  }
inline void Stub_Columns(int count, const char* id, bool borders) { Hit(253, "Columns");  }
inline void Stub_NextColumn() { Hit(254, "NextColumn");  }
inline int Stub_GetColumnIndex() { Hit(255, "GetColumnIndex"); return {}; }
inline float Stub_GetColumnWidth(int column_index) { Hit(256, "GetColumnWidth"); return {}; }
inline void Stub_SetColumnWidth(int column_index, float width) { Hit(257, "SetColumnWidth");  }
inline float Stub_GetColumnOffset(int column_index) { Hit(258, "GetColumnOffset"); return {}; }
inline void Stub_SetColumnOffset(int column_index, float offset_x) { Hit(259, "SetColumnOffset");  }
inline int Stub_GetColumnsCount() { Hit(260, "GetColumnsCount"); return {}; }
inline bool Stub_BeginTabBar(const char* str_id, ImGuiTabBarFlags flags) { Hit(261, "BeginTabBar"); return true; }
inline void Stub_EndTabBar() { Hit(262, "EndTabBar");  }
inline bool Stub_BeginTabItem(const char* label, bool* p_open, ImGuiTabItemFlags flags) { Hit(263, "BeginTabItem"); return true; }
inline void Stub_EndTabItem() { Hit(264, "EndTabItem");  }
inline bool Stub_TabItemButton(const char* label, ImGuiTabItemFlags flags) { Hit(265, "TabItemButton"); return {}; }
inline void Stub_SetTabItemClosed(const char* tab_or_docked_window_label) { Hit(266, "SetTabItemClosed");  }
inline ImGuiID Stub_DockSpace(ImGuiID dockspace_id, const ImVec2& size, ImGuiDockNodeFlags flags, const ImGuiWindowClass* window_class) { Hit(267, "DockSpace"); return {}; }
inline void Stub_SetNextWindowDockID(ImGuiID dock_id, ImGuiCond cond) { Hit(268, "SetNextWindowDockID");  }
inline void Stub_SetNextWindowClass(const ImGuiWindowClass* window_class) { Hit(269, "SetNextWindowClass");  }
inline ImGuiID Stub_GetWindowDockID() { Hit(270, "GetWindowDockID"); return {}; }
inline bool Stub_IsWindowDocked() { Hit(271, "IsWindowDocked"); return {}; }
inline bool Stub_BeginDragDropSource(ImGuiDragDropFlags flags) { Hit(272, "BeginDragDropSource"); return {}; }
inline bool Stub_SetDragDropPayload(const char* type, const void* data, size_t sz, ImGuiCond cond) { Hit(273, "SetDragDropPayload"); return {}; }
inline void Stub_EndDragDropSource() { Hit(274, "EndDragDropSource");  }
inline bool Stub_BeginDragDropTarget() { Hit(275, "BeginDragDropTarget"); return {}; }
inline const ImGuiPayload* Stub_AcceptDragDropPayload(const char* type, ImGuiDragDropFlags flags) { Hit(276, "AcceptDragDropPayload"); return {}; }
inline void Stub_EndDragDropTarget() { Hit(277, "EndDragDropTarget");  }
inline const ImGuiPayload* Stub_GetDragDropPayload() { Hit(278, "GetDragDropPayload"); return {}; }
inline void Stub_BeginDisabled(bool disabled) { Hit(279, "BeginDisabled");  }
inline void Stub_EndDisabled() { Hit(280, "EndDisabled");  }
inline void Stub_PushClipRect(const ImVec2& clip_rect_min, const ImVec2& clip_rect_max, bool intersect_with_current_clip_rect) { Hit(281, "PushClipRect");  }
inline void Stub_PopClipRect() { Hit(282, "PopClipRect");  }
inline void Stub_SetItemDefaultFocus() { Hit(283, "SetItemDefaultFocus");  }
inline void Stub_SetKeyboardFocusHere(int offset) { Hit(284, "SetKeyboardFocusHere");  }
inline void Stub_SetNavCursorVisible(bool visible) { Hit(285, "SetNavCursorVisible");  }
inline void Stub_SetNextItemAllowOverlap() { Hit(286, "SetNextItemAllowOverlap");  }
inline bool Stub_IsItemHovered(ImGuiHoveredFlags flags) { Hit(287, "IsItemHovered"); return {}; }
inline bool Stub_IsItemActive() { Hit(288, "IsItemActive"); return {}; }
inline bool Stub_IsItemFocused() { Hit(289, "IsItemFocused"); return {}; }
inline bool Stub_IsItemClicked(ImGuiMouseButton mouse_button) { Hit(290, "IsItemClicked"); return {}; }
inline bool Stub_IsItemVisible() { Hit(291, "IsItemVisible"); return {}; }
inline bool Stub_IsItemEdited() { Hit(292, "IsItemEdited"); return {}; }
inline bool Stub_IsItemActivated() { Hit(293, "IsItemActivated"); return {}; }
inline bool Stub_IsItemDeactivated() { Hit(294, "IsItemDeactivated"); return {}; }
inline bool Stub_IsItemDeactivatedAfterEdit() { Hit(295, "IsItemDeactivatedAfterEdit"); return {}; }
inline bool Stub_IsItemToggledOpen() { Hit(296, "IsItemToggledOpen"); return {}; }
inline bool Stub_IsAnyItemHovered() { Hit(297, "IsAnyItemHovered"); return {}; }
inline bool Stub_IsAnyItemActive() { Hit(298, "IsAnyItemActive"); return {}; }
inline bool Stub_IsAnyItemFocused() { Hit(299, "IsAnyItemFocused"); return {}; }
inline ImGuiID Stub_GetItemID() { Hit(300, "GetItemID"); return {}; }
inline ImVec2 Stub_GetItemRectMin() { Hit(301, "GetItemRectMin"); return ImVec2(100.0f, 100.0f); }
inline ImVec2 Stub_GetItemRectMax() { Hit(302, "GetItemRectMax"); return ImVec2(200.0f, 122.0f); }
inline ImVec2 Stub_GetItemRectSize() { Hit(303, "GetItemRectSize"); return ImVec2(100.0f, 22.0f); }
inline ImDrawList* Stub_GetBackgroundDrawList(ImGuiViewport* viewport) { Hit(304, "GetBackgroundDrawList"); return List(); }
inline ImDrawList* Stub_GetForegroundDrawList(ImGuiViewport* viewport) { Hit(305, "GetForegroundDrawList"); return List(); }
inline bool Stub_IsRectVisible(const ImVec2& size) { Hit(306, "IsRectVisible"); return {}; }
inline bool Stub_IsRectVisible2(const ImVec2& rect_min, const ImVec2& rect_max) { Hit(307, "IsRectVisible2"); return {}; }
inline double Stub_GetTime() { Hit(308, "GetTime"); return Seconds(); }
inline int Stub_GetFrameCount() { Hit(309, "GetFrameCount"); return Frames(); }
inline ImDrawListSharedData* Stub_GetDrawListSharedData() { Hit(310, "GetDrawListSharedData"); return {}; }
inline const char* Stub_GetStyleColorName(ImGuiCol idx) { Hit(311, "GetStyleColorName"); return {}; }
inline void Stub_SetStateStorage(ImGuiStorage* storage) { Hit(312, "SetStateStorage");  }
inline ImGuiStorage* Stub_GetStateStorage() { Hit(313, "GetStateStorage"); return {}; }
inline ImVec2 Stub_CalcTextSize(const char* text, const char* text_end, bool hide_text_after_double_hash, float wrap_width) { Hit(314, "CalcTextSize"); size_t n = 0; for (const char *w = text; w && (text_end == nullptr || w < text_end) && *w; ++w) if ((static_cast<unsigned char>(*w) & 0xC0) != 0x80) ++n; return ImVec2(static_cast<float>(n) * 7.0f, 13.0f); }
inline ImVec4 Stub_ColorConvertU32ToFloat4(ImU32 in) { Hit(315, "ColorConvertU32ToFloat4"); return {}; }
inline ImU32 Stub_ColorConvertFloat4ToU32(const ImVec4& in) { Hit(316, "ColorConvertFloat4ToU32"); return {}; }
inline void Stub_ColorConvertRGBtoHSV(float r, float g, float b, float& out_h, float& out_s, float& out_v) { Hit(317, "ColorConvertRGBtoHSV");  }
inline void Stub_ColorConvertHSVtoRGB(float h, float s, float v, float& out_r, float& out_g, float& out_b) { Hit(318, "ColorConvertHSVtoRGB");  }
inline bool Stub_IsKeyDown(ImGuiKey key) { Hit(319, "IsKeyDown"); return {}; }
inline bool Stub_IsKeyPressed(ImGuiKey key, bool repeat) { Hit(320, "IsKeyPressed"); return {}; }
inline bool Stub_IsKeyReleased(ImGuiKey key) { Hit(321, "IsKeyReleased"); return {}; }
inline bool Stub_IsKeyChordPressed(ImGuiKeyChord key_chord) { Hit(322, "IsKeyChordPressed"); return {}; }
inline int Stub_GetKeyPressedAmount(ImGuiKey key, float repeat_delay, float rate) { Hit(323, "GetKeyPressedAmount"); return {}; }
inline const char* Stub_GetKeyName(ImGuiKey key) { Hit(324, "GetKeyName"); return ""; }
inline void Stub_SetNextFrameWantCaptureKeyboard(bool want_capture_keyboard) { Hit(325, "SetNextFrameWantCaptureKeyboard");  }
inline bool Stub_Shortcut(ImGuiKeyChord key_chord, ImGuiInputFlags flags) { Hit(326, "Shortcut"); return {}; }
inline void Stub_SetNextItemShortcut(ImGuiKeyChord key_chord, ImGuiInputFlags flags) { Hit(327, "SetNextItemShortcut");  }
inline void Stub_SetItemKeyOwner(ImGuiKey key) { Hit(328, "SetItemKeyOwner");  }
inline bool Stub_IsMouseDown(ImGuiMouseButton button) { Hit(329, "IsMouseDown"); return {}; }
inline bool Stub_IsMouseClicked(ImGuiMouseButton button, bool repeat) { Hit(330, "IsMouseClicked"); return {}; }
inline bool Stub_IsMouseReleased(ImGuiMouseButton button) { Hit(331, "IsMouseReleased"); return {}; }
inline bool Stub_IsMouseDoubleClicked(ImGuiMouseButton button) { Hit(332, "IsMouseDoubleClicked"); return {}; }
inline bool Stub_IsMouseReleasedWithDelay(ImGuiMouseButton button, float delay) { Hit(333, "IsMouseReleasedWithDelay"); return {}; }
inline int Stub_GetMouseClickedCount(ImGuiMouseButton button) { Hit(334, "GetMouseClickedCount"); return {}; }
inline bool Stub_IsMouseHoveringRect(const ImVec2& r_min, const ImVec2& r_max, bool clip) { Hit(335, "IsMouseHoveringRect"); return {}; }
inline bool Stub_IsMousePosValid(const ImVec2* mouse_pos) { Hit(336, "IsMousePosValid"); return {}; }
inline bool Stub_IsAnyMouseDown() { Hit(337, "IsAnyMouseDown"); return {}; }
inline ImVec2 Stub_GetMousePos() { Hit(338, "GetMousePos"); return {}; }
inline ImVec2 Stub_GetMousePosOnOpeningCurrentPopup() { Hit(339, "GetMousePosOnOpeningCurrentPopup"); return {}; }
inline bool Stub_IsMouseDragging(ImGuiMouseButton button, float lock_threshold) { Hit(340, "IsMouseDragging"); return {}; }
inline ImVec2 Stub_GetMouseDragDelta(ImGuiMouseButton button, float lock_threshold) { Hit(341, "GetMouseDragDelta"); return {}; }
inline void Stub_ResetMouseDragDelta(ImGuiMouseButton button) { Hit(342, "ResetMouseDragDelta");  }
inline ImGuiMouseCursor Stub_GetMouseCursor() { Hit(343, "GetMouseCursor"); return {}; }
inline void Stub_SetMouseCursor(ImGuiMouseCursor cursor_type) { Hit(344, "SetMouseCursor");  }
inline void Stub_SetNextFrameWantCaptureMouse(bool want_capture_mouse) { Hit(345, "SetNextFrameWantCaptureMouse");  }
inline const char* Stub_GetClipboardText() { Hit(346, "GetClipboardText"); return ""; }
inline void Stub_SetClipboardText(const char* text) { Hit(347, "SetClipboardText");  }
inline void Stub_SetAllocatorFunctions(ImGuiMemAllocFunc alloc_func, ImGuiMemFreeFunc free_func, void* user_data) { Hit(348, "SetAllocatorFunctions");  }
inline void Stub_GetAllocatorFunctions(ImGuiMemAllocFunc* p_alloc_func, ImGuiMemFreeFunc* p_free_func, void** p_user_data) { Hit(349, "GetAllocatorFunctions");  }
inline void* Stub_MemAlloc(size_t size) { Hit(350, "MemAlloc"); return std::malloc(size ? size : 1); }
inline void Stub_MemFree(void* ptr) { Hit(351, "MemFree"); std::free(ptr); }
inline int Stub_ImGuiStorage_GetInt(const ImGuiStorage *_this, ImGuiID key, int default_val) { Hit(352, "ImGuiStorage_GetInt"); return {}; }
inline void Stub_ImGuiStorage_SetInt(ImGuiStorage *_this, ImGuiID key, int val) { Hit(353, "ImGuiStorage_SetInt");  }
inline bool Stub_ImGuiStorage_GetBool(const ImGuiStorage *_this, ImGuiID key, bool default_val) { Hit(354, "ImGuiStorage_GetBool"); return {}; }
inline void Stub_ImGuiStorage_SetBool(ImGuiStorage *_this, ImGuiID key, bool val) { Hit(355, "ImGuiStorage_SetBool");  }
inline float Stub_ImGuiStorage_GetFloat(const ImGuiStorage *_this, ImGuiID key, float default_val) { Hit(356, "ImGuiStorage_GetFloat"); return {}; }
inline void Stub_ImGuiStorage_SetFloat(ImGuiStorage *_this, ImGuiID key, float val) { Hit(357, "ImGuiStorage_SetFloat");  }
inline void* Stub_ImGuiStorage_GetVoidPtr(const ImGuiStorage *_this, ImGuiID key) { Hit(358, "ImGuiStorage_GetVoidPtr"); return {}; }
inline void Stub_ImGuiStorage_SetVoidPtr(ImGuiStorage *_this, ImGuiID key, void* val) { Hit(359, "ImGuiStorage_SetVoidPtr");  }
inline int* Stub_ImGuiStorage_GetIntRef(ImGuiStorage *_this, ImGuiID key, int default_val) { Hit(360, "ImGuiStorage_GetIntRef"); return {}; }
inline bool* Stub_ImGuiStorage_GetBoolRef(ImGuiStorage *_this, ImGuiID key, bool default_val) { Hit(361, "ImGuiStorage_GetBoolRef"); return {}; }
inline float* Stub_ImGuiStorage_GetFloatRef(ImGuiStorage *_this, ImGuiID key, float default_val) { Hit(362, "ImGuiStorage_GetFloatRef"); return {}; }
inline void** Stub_ImGuiStorage_GetVoidPtrRef(ImGuiStorage *_this, ImGuiID key, void* default_val) { Hit(363, "ImGuiStorage_GetVoidPtrRef"); return {}; }
inline void Stub_ImGuiStorage_BuildSortByKey(ImGuiStorage *_this) { Hit(364, "ImGuiStorage_BuildSortByKey");  }
inline void Stub_ImGuiStorage_SetAllInt(ImGuiStorage *_this, int val) { Hit(365, "ImGuiStorage_SetAllInt");  }
inline void Stub_ConstructImGuiListClipper(ImGuiListClipper *_this) { Hit(366, "ConstructImGuiListClipper");  }
inline void Stub_DestructImGuiListClipper(ImGuiListClipper *_this) { Hit(367, "DestructImGuiListClipper");  }
inline void Stub_ImGuiListClipper_Begin(ImGuiListClipper *_this, int items_count, float items_height) { Hit(368, "ImGuiListClipper_Begin");  }
inline void Stub_ImGuiListClipper_End(ImGuiListClipper *_this) { Hit(369, "ImGuiListClipper_End");  }
inline bool Stub_ImGuiListClipper_Step(ImGuiListClipper *_this) { Hit(370, "ImGuiListClipper_Step"); return {}; }
inline void Stub_ImGuiListClipper_IncludeItemsByIndex(ImGuiListClipper *_this, int item_begin, int item_end) { Hit(371, "ImGuiListClipper_IncludeItemsByIndex");  }
inline void Stub_ImGuiListClipper_SeekCursorForItem(ImGuiListClipper *_this, int item_index) { Hit(372, "ImGuiListClipper_SeekCursorForItem");  }
inline void Stub_ConstructImDrawList(ImDrawList *_this, ImDrawListSharedData* shared_data) { Hit(373, "ConstructImDrawList");  }
inline void Stub_DestructImDrawList(ImDrawList *_this) { Hit(374, "DestructImDrawList");  }
inline void Stub_ImDrawList_PushClipRect(ImDrawList *_this, const ImVec2& clip_rect_min, const ImVec2& clip_rect_max, bool intersect_with_current_clip_rect) { Hit(375, "ImDrawList_PushClipRect");  }
inline void Stub_ImDrawList_PushClipRectFullScreen(ImDrawList *_this) { Hit(376, "ImDrawList_PushClipRectFullScreen");  }
inline void Stub_ImDrawList_PopClipRect(ImDrawList *_this) { Hit(377, "ImDrawList_PopClipRect");  }
inline void Stub_ImDrawList_PushTexture(ImDrawList *_this, ImTextureRef tex_ref) { Hit(378, "ImDrawList_PushTexture");  }
inline void Stub_ImDrawList_PopTexture(ImDrawList *_this) { Hit(379, "ImDrawList_PopTexture");  }
inline void Stub_ImDrawList_AddLine(ImDrawList *_this, const ImVec2& p1, const ImVec2& p2, ImU32 col, float thickness) { Hit(380, "ImDrawList_AddLine");  }
inline void Stub_ImDrawList_AddRect(ImDrawList *_this, const ImVec2& p_min, const ImVec2& p_max, ImU32 col, float rounding, ImDrawFlags flags, float thickness) { Hit(381, "ImDrawList_AddRect");  }
inline void Stub_ImDrawList_AddRectFilled(ImDrawList *_this, const ImVec2& p_min, const ImVec2& p_max, ImU32 col, float rounding, ImDrawFlags flags) { Hit(382, "ImDrawList_AddRectFilled");  }
inline void Stub_ImDrawList_AddRectFilledMultiColor(ImDrawList *_this, const ImVec2& p_min, const ImVec2& p_max, ImU32 col_upr_left, ImU32 col_upr_right, ImU32 col_bot_right, ImU32 col_bot_left) { Hit(383, "ImDrawList_AddRectFilledMultiColor");  }
inline void Stub_ImDrawList_AddQuad(ImDrawList *_this, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, ImU32 col, float thickness) { Hit(384, "ImDrawList_AddQuad");  }
inline void Stub_ImDrawList_AddQuadFilled(ImDrawList *_this, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, ImU32 col) { Hit(385, "ImDrawList_AddQuadFilled");  }
inline void Stub_ImDrawList_AddTriangle(ImDrawList *_this, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, ImU32 col, float thickness) { Hit(386, "ImDrawList_AddTriangle");  }
inline void Stub_ImDrawList_AddTriangleFilled(ImDrawList *_this, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, ImU32 col) { Hit(387, "ImDrawList_AddTriangleFilled");  }
inline void Stub_ImDrawList_AddCircle(ImDrawList *_this, const ImVec2& center, float radius, ImU32 col, int num_segments, float thickness) { Hit(388, "ImDrawList_AddCircle");  }
inline void Stub_ImDrawList_AddCircleFilled(ImDrawList *_this, const ImVec2& center, float radius, ImU32 col, int num_segments) { Hit(389, "ImDrawList_AddCircleFilled");  }
inline void Stub_ImDrawList_AddNgon(ImDrawList *_this, const ImVec2& center, float radius, ImU32 col, int num_segments, float thickness) { Hit(390, "ImDrawList_AddNgon");  }
inline void Stub_ImDrawList_AddNgonFilled(ImDrawList *_this, const ImVec2& center, float radius, ImU32 col, int num_segments) { Hit(391, "ImDrawList_AddNgonFilled");  }
inline void Stub_ImDrawList_AddEllipse(ImDrawList *_this, const ImVec2& center, const ImVec2& radius, ImU32 col, float rot, int num_segments, float thickness) { Hit(392, "ImDrawList_AddEllipse");  }
inline void Stub_ImDrawList_AddEllipseFilled(ImDrawList *_this, const ImVec2& center, const ImVec2& radius, ImU32 col, float rot, int num_segments) { Hit(393, "ImDrawList_AddEllipseFilled");  }
inline void Stub_ImDrawList_AddText(ImDrawList *_this, const ImVec2& pos, ImU32 col, const char* text_begin, const char* text_end) { Hit(394, "ImDrawList_AddText");  }
inline void Stub_ImDrawList_AddText2(ImDrawList *_this, ImFont* font, float font_size, const ImVec2& pos, ImU32 col, const char* text_begin, const char* text_end, float wrap_width, const ImVec4* cpu_fine_clip_rect) { Hit(395, "ImDrawList_AddText2");  }
inline void Stub_ImDrawList_AddBezierCubic(ImDrawList *_this, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, ImU32 col, float thickness, int num_segments) { Hit(396, "ImDrawList_AddBezierCubic");  }
inline void Stub_ImDrawList_AddBezierQuadratic(ImDrawList *_this, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, ImU32 col, float thickness, int num_segments) { Hit(397, "ImDrawList_AddBezierQuadratic");  }
inline void Stub_ImDrawList_AddPolyline(ImDrawList *_this, const ImVec2* points, int num_points, ImU32 col, ImDrawFlags flags, float thickness) { Hit(398, "ImDrawList_AddPolyline");  }
inline void Stub_ImDrawList_AddConvexPolyFilled(ImDrawList *_this, const ImVec2* points, int num_points, ImU32 col) { Hit(399, "ImDrawList_AddConvexPolyFilled");  }
inline void Stub_ImDrawList_AddConcavePolyFilled(ImDrawList *_this, const ImVec2* points, int num_points, ImU32 col) { Hit(400, "ImDrawList_AddConcavePolyFilled");  }
inline void Stub_ImDrawList_AddImage(ImDrawList *_this, ImTextureRef tex_ref, const ImVec2& p_min, const ImVec2& p_max, const ImVec2& uv_min, const ImVec2& uv_max, ImU32 col) { Hit(401, "ImDrawList_AddImage");  }
inline void Stub_ImDrawList_AddImageQuad(ImDrawList *_this, ImTextureRef tex_ref, const ImVec2& p1, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, const ImVec2& uv1, const ImVec2& uv2, const ImVec2& uv3, const ImVec2& uv4, ImU32 col) { Hit(402, "ImDrawList_AddImageQuad");  }
inline void Stub_ImDrawList_AddImageRounded(ImDrawList *_this, ImTextureRef tex_ref, const ImVec2& p_min, const ImVec2& p_max, const ImVec2& uv_min, const ImVec2& uv_max, ImU32 col, float rounding, ImDrawFlags flags) { Hit(403, "ImDrawList_AddImageRounded");  }
inline void Stub_ImDrawList_PathArcTo(ImDrawList *_this, const ImVec2& center, float radius, float a_min, float a_max, int num_segments) { Hit(404, "ImDrawList_PathArcTo");  }
inline void Stub_ImDrawList_PathArcToFast(ImDrawList *_this, const ImVec2& center, float radius, int a_min_of_12, int a_max_of_12) { Hit(405, "ImDrawList_PathArcToFast");  }
inline void Stub_ImDrawList_PathEllipticalArcTo(ImDrawList *_this, const ImVec2& center, const ImVec2& radius, float rot, float a_min, float a_max, int num_segments) { Hit(406, "ImDrawList_PathEllipticalArcTo");  }
inline void Stub_ImDrawList_PathBezierCubicCurveTo(ImDrawList *_this, const ImVec2& p2, const ImVec2& p3, const ImVec2& p4, int num_segments) { Hit(407, "ImDrawList_PathBezierCubicCurveTo");  }
inline void Stub_ImDrawList_PathBezierQuadraticCurveTo(ImDrawList *_this, const ImVec2& p2, const ImVec2& p3, int num_segments) { Hit(408, "ImDrawList_PathBezierQuadraticCurveTo");  }
inline void Stub_ImDrawList_PathRect(ImDrawList *_this, const ImVec2& rect_min, const ImVec2& rect_max, float rounding, ImDrawFlags flags) { Hit(409, "ImDrawList_PathRect");  }
inline void Stub_ImDrawList_AddCallback(ImDrawList *_this, ImDrawCallback callback, void* userdata, size_t userdata_size) { Hit(410, "ImDrawList_AddCallback");  }
inline void Stub_ImDrawList_AddDrawCmd(ImDrawList *_this) { Hit(411, "ImDrawList_AddDrawCmd");  }
inline ImDrawList* Stub_ImDrawList_CloneOutput(const ImDrawList *_this) { Hit(412, "ImDrawList_CloneOutput"); return {}; }
inline void Stub_ImDrawList_PrimReserve(ImDrawList *_this, int idx_count, int vtx_count) { Hit(413, "ImDrawList_PrimReserve");  }
inline void Stub_ImDrawList_PrimUnreserve(ImDrawList *_this, int idx_count, int vtx_count) { Hit(414, "ImDrawList_PrimUnreserve");  }
inline void Stub_ImDrawList_PrimRect(ImDrawList *_this, const ImVec2& a, const ImVec2& b, ImU32 col) { Hit(415, "ImDrawList_PrimRect");  }
inline void Stub_ImDrawList_PrimRectUV(ImDrawList *_this, const ImVec2& a, const ImVec2& b, const ImVec2& uv_a, const ImVec2& uv_b, ImU32 col) { Hit(416, "ImDrawList_PrimRectUV");  }
inline void Stub_ImDrawList_PrimQuadUV(ImDrawList *_this, const ImVec2& a, const ImVec2& b, const ImVec2& c, const ImVec2& d, const ImVec2& uv_a, const ImVec2& uv_b, const ImVec2& uv_c, const ImVec2& uv_d, ImU32 col) { Hit(417, "ImDrawList_PrimQuadUV");  }
inline void Stub_ConstructImFont(ImFont *_this) { Hit(418, "ConstructImFont");  }
inline void Stub_DestructImFont(ImFont *_this) { Hit(419, "DestructImFont");  }
inline bool Stub_ImFont_IsGlyphInFont(ImFont *_this, ImWchar c) { Hit(420, "ImFont_IsGlyphInFont"); return true; }
inline void Fill(imgui_function_table_19250 &t)
{
    t.GetIO = &Stub_GetIO;
    t.GetStyle = &Stub_GetStyle;
    t.GetVersion = &Stub_GetVersion;
    t.Begin = &Stub_Begin;
    t.End = &Stub_End;
    t.BeginChild = &Stub_BeginChild;
    t.BeginChild2 = &Stub_BeginChild2;
    t.EndChild = &Stub_EndChild;
    t.IsWindowAppearing = &Stub_IsWindowAppearing;
    t.IsWindowCollapsed = &Stub_IsWindowCollapsed;
    t.IsWindowFocused = &Stub_IsWindowFocused;
    t.IsWindowHovered = &Stub_IsWindowHovered;
    t.GetWindowDrawList = &Stub_GetWindowDrawList;
    t.GetWindowDpiScale = &Stub_GetWindowDpiScale;
    t.GetWindowPos = &Stub_GetWindowPos;
    t.GetWindowSize = &Stub_GetWindowSize;
    t.GetWindowWidth = &Stub_GetWindowWidth;
    t.GetWindowHeight = &Stub_GetWindowHeight;
    t.SetNextWindowPos = &Stub_SetNextWindowPos;
    t.SetNextWindowSize = &Stub_SetNextWindowSize;
    t.SetNextWindowSizeConstraints = &Stub_SetNextWindowSizeConstraints;
    t.SetNextWindowContentSize = &Stub_SetNextWindowContentSize;
    t.SetNextWindowCollapsed = &Stub_SetNextWindowCollapsed;
    t.SetNextWindowFocus = &Stub_SetNextWindowFocus;
    t.SetNextWindowScroll = &Stub_SetNextWindowScroll;
    t.SetNextWindowBgAlpha = &Stub_SetNextWindowBgAlpha;
    t.SetWindowPos = &Stub_SetWindowPos;
    t.SetWindowSize = &Stub_SetWindowSize;
    t.SetWindowCollapsed = &Stub_SetWindowCollapsed;
    t.SetWindowFocus = &Stub_SetWindowFocus;
    t.SetWindowPos2 = &Stub_SetWindowPos2;
    t.SetWindowSize2 = &Stub_SetWindowSize2;
    t.SetWindowCollapsed2 = &Stub_SetWindowCollapsed2;
    t.SetWindowFocus2 = &Stub_SetWindowFocus2;
    t.GetScrollX = &Stub_GetScrollX;
    t.GetScrollY = &Stub_GetScrollY;
    t.SetScrollX = &Stub_SetScrollX;
    t.SetScrollY = &Stub_SetScrollY;
    t.GetScrollMaxX = &Stub_GetScrollMaxX;
    t.GetScrollMaxY = &Stub_GetScrollMaxY;
    t.SetScrollHereX = &Stub_SetScrollHereX;
    t.SetScrollHereY = &Stub_SetScrollHereY;
    t.SetScrollFromPosX = &Stub_SetScrollFromPosX;
    t.SetScrollFromPosY = &Stub_SetScrollFromPosY;
    t.PushFont = &Stub_PushFont;
    t.PopFont = &Stub_PopFont;
    t.GetFont = &Stub_GetFont;
    t.GetFontSize = &Stub_GetFontSize;
    t.GetFontBaked = &Stub_GetFontBaked;
    t.PushStyleColor = &Stub_PushStyleColor;
    t.PushStyleColor2 = &Stub_PushStyleColor2;
    t.PopStyleColor = &Stub_PopStyleColor;
    t.PushStyleVar = &Stub_PushStyleVar;
    t.PushStyleVar2 = &Stub_PushStyleVar2;
    t.PushStyleVarX = &Stub_PushStyleVarX;
    t.PushStyleVarY = &Stub_PushStyleVarY;
    t.PopStyleVar = &Stub_PopStyleVar;
    t.PushItemFlag = &Stub_PushItemFlag;
    t.PopItemFlag = &Stub_PopItemFlag;
    t.PushItemWidth = &Stub_PushItemWidth;
    t.PopItemWidth = &Stub_PopItemWidth;
    t.SetNextItemWidth = &Stub_SetNextItemWidth;
    t.CalcItemWidth = &Stub_CalcItemWidth;
    t.PushTextWrapPos = &Stub_PushTextWrapPos;
    t.PopTextWrapPos = &Stub_PopTextWrapPos;
    t.GetFontTexUvWhitePixel = &Stub_GetFontTexUvWhitePixel;
    t.GetColorU32 = &Stub_GetColorU32;
    t.GetColorU322 = &Stub_GetColorU322;
    t.GetColorU323 = &Stub_GetColorU323;
    t.GetStyleColorVec4 = &Stub_GetStyleColorVec4;
    t.GetCursorScreenPos = &Stub_GetCursorScreenPos;
    t.SetCursorScreenPos = &Stub_SetCursorScreenPos;
    t.GetContentRegionAvail = &Stub_GetContentRegionAvail;
    t.GetCursorPos = &Stub_GetCursorPos;
    t.GetCursorPosX = &Stub_GetCursorPosX;
    t.GetCursorPosY = &Stub_GetCursorPosY;
    t.SetCursorPos = &Stub_SetCursorPos;
    t.SetCursorPosX = &Stub_SetCursorPosX;
    t.SetCursorPosY = &Stub_SetCursorPosY;
    t.GetCursorStartPos = &Stub_GetCursorStartPos;
    t.Separator = &Stub_Separator;
    t.SameLine = &Stub_SameLine;
    t.NewLine = &Stub_NewLine;
    t.Spacing = &Stub_Spacing;
    t.Dummy = &Stub_Dummy;
    t.Indent = &Stub_Indent;
    t.Unindent = &Stub_Unindent;
    t.BeginGroup = &Stub_BeginGroup;
    t.EndGroup = &Stub_EndGroup;
    t.AlignTextToFramePadding = &Stub_AlignTextToFramePadding;
    t.GetTextLineHeight = &Stub_GetTextLineHeight;
    t.GetTextLineHeightWithSpacing = &Stub_GetTextLineHeightWithSpacing;
    t.GetFrameHeight = &Stub_GetFrameHeight;
    t.GetFrameHeightWithSpacing = &Stub_GetFrameHeightWithSpacing;
    t.PushID = &Stub_PushID;
    t.PushID2 = &Stub_PushID2;
    t.PushID3 = &Stub_PushID3;
    t.PushID4 = &Stub_PushID4;
    t.PopID = &Stub_PopID;
    t.GetID = &Stub_GetID;
    t.GetID2 = &Stub_GetID2;
    t.GetID3 = &Stub_GetID3;
    t.GetID4 = &Stub_GetID4;
    t.TextUnformatted = &Stub_TextUnformatted;
    t.TextV = &Stub_TextV;
    t.TextColoredV = &Stub_TextColoredV;
    t.TextDisabledV = &Stub_TextDisabledV;
    t.TextWrappedV = &Stub_TextWrappedV;
    t.LabelTextV = &Stub_LabelTextV;
    t.BulletTextV = &Stub_BulletTextV;
    t.SeparatorText = &Stub_SeparatorText;
    t.Button = &Stub_Button;
    t.SmallButton = &Stub_SmallButton;
    t.InvisibleButton = &Stub_InvisibleButton;
    t.ArrowButton = &Stub_ArrowButton;
    t.Checkbox = &Stub_Checkbox;
    t.CheckboxFlags = &Stub_CheckboxFlags;
    t.CheckboxFlags2 = &Stub_CheckboxFlags2;
    t.RadioButton = &Stub_RadioButton;
    t.RadioButton2 = &Stub_RadioButton2;
    t.ProgressBar = &Stub_ProgressBar;
    t.Bullet = &Stub_Bullet;
    t.TextLink = &Stub_TextLink;
    t.TextLinkOpenURL = &Stub_TextLinkOpenURL;
    t.Image = &Stub_Image;
    t.ImageWithBg = &Stub_ImageWithBg;
    t.ImageButton = &Stub_ImageButton;
    t.BeginCombo = &Stub_BeginCombo;
    t.EndCombo = &Stub_EndCombo;
    t.Combo = &Stub_Combo;
    t.Combo2 = &Stub_Combo2;
    t.Combo3 = &Stub_Combo3;
    t.DragFloat = &Stub_DragFloat;
    t.DragFloat2 = &Stub_DragFloat2;
    t.DragFloat3 = &Stub_DragFloat3;
    t.DragFloat4 = &Stub_DragFloat4;
    t.DragFloatRange2 = &Stub_DragFloatRange2;
    t.DragInt = &Stub_DragInt;
    t.DragInt2 = &Stub_DragInt2;
    t.DragInt3 = &Stub_DragInt3;
    t.DragInt4 = &Stub_DragInt4;
    t.DragIntRange2 = &Stub_DragIntRange2;
    t.DragScalar = &Stub_DragScalar;
    t.DragScalarN = &Stub_DragScalarN;
    t.SliderFloat = &Stub_SliderFloat;
    t.SliderFloat2 = &Stub_SliderFloat2;
    t.SliderFloat3 = &Stub_SliderFloat3;
    t.SliderFloat4 = &Stub_SliderFloat4;
    t.SliderAngle = &Stub_SliderAngle;
    t.SliderInt = &Stub_SliderInt;
    t.SliderInt2 = &Stub_SliderInt2;
    t.SliderInt3 = &Stub_SliderInt3;
    t.SliderInt4 = &Stub_SliderInt4;
    t.SliderScalar = &Stub_SliderScalar;
    t.SliderScalarN = &Stub_SliderScalarN;
    t.VSliderFloat = &Stub_VSliderFloat;
    t.VSliderInt = &Stub_VSliderInt;
    t.VSliderScalar = &Stub_VSliderScalar;
    t.InputText = &Stub_InputText;
    t.InputTextMultiline = &Stub_InputTextMultiline;
    t.InputTextWithHint = &Stub_InputTextWithHint;
    t.InputFloat = &Stub_InputFloat;
    t.InputFloat2 = &Stub_InputFloat2;
    t.InputFloat3 = &Stub_InputFloat3;
    t.InputFloat4 = &Stub_InputFloat4;
    t.InputInt = &Stub_InputInt;
    t.InputInt2 = &Stub_InputInt2;
    t.InputInt3 = &Stub_InputInt3;
    t.InputInt4 = &Stub_InputInt4;
    t.InputDouble = &Stub_InputDouble;
    t.InputScalar = &Stub_InputScalar;
    t.InputScalarN = &Stub_InputScalarN;
    t.ColorEdit3 = &Stub_ColorEdit3;
    t.ColorEdit4 = &Stub_ColorEdit4;
    t.ColorPicker3 = &Stub_ColorPicker3;
    t.ColorPicker4 = &Stub_ColorPicker4;
    t.ColorButton = &Stub_ColorButton;
    t.SetColorEditOptions = &Stub_SetColorEditOptions;
    t.TreeNode = &Stub_TreeNode;
    t.TreeNodeV = &Stub_TreeNodeV;
    t.TreeNodeV2 = &Stub_TreeNodeV2;
    t.TreeNodeEx = &Stub_TreeNodeEx;
    t.TreeNodeExV = &Stub_TreeNodeExV;
    t.TreeNodeExV2 = &Stub_TreeNodeExV2;
    t.TreePush = &Stub_TreePush;
    t.TreePush2 = &Stub_TreePush2;
    t.TreePop = &Stub_TreePop;
    t.GetTreeNodeToLabelSpacing = &Stub_GetTreeNodeToLabelSpacing;
    t.CollapsingHeader = &Stub_CollapsingHeader;
    t.CollapsingHeader2 = &Stub_CollapsingHeader2;
    t.SetNextItemOpen = &Stub_SetNextItemOpen;
    t.SetNextItemStorageID = &Stub_SetNextItemStorageID;
    t.Selectable = &Stub_Selectable;
    t.Selectable2 = &Stub_Selectable2;
    t.BeginMultiSelect = &Stub_BeginMultiSelect;
    t.EndMultiSelect = &Stub_EndMultiSelect;
    t.SetNextItemSelectionUserData = &Stub_SetNextItemSelectionUserData;
    t.IsItemToggledSelection = &Stub_IsItemToggledSelection;
    t.BeginListBox = &Stub_BeginListBox;
    t.EndListBox = &Stub_EndListBox;
    t.ListBox = &Stub_ListBox;
    t.ListBox2 = &Stub_ListBox2;
    t.PlotLines = &Stub_PlotLines;
    t.PlotLines2 = &Stub_PlotLines2;
    t.PlotHistogram = &Stub_PlotHistogram;
    t.PlotHistogram2 = &Stub_PlotHistogram2;
    t.Value = &Stub_Value;
    t.Value2 = &Stub_Value2;
    t.Value3 = &Stub_Value3;
    t.Value4 = &Stub_Value4;
    t.BeginMenuBar = &Stub_BeginMenuBar;
    t.EndMenuBar = &Stub_EndMenuBar;
    t.BeginMainMenuBar = &Stub_BeginMainMenuBar;
    t.EndMainMenuBar = &Stub_EndMainMenuBar;
    t.BeginMenu = &Stub_BeginMenu;
    t.EndMenu = &Stub_EndMenu;
    t.MenuItem = &Stub_MenuItem;
    t.MenuItem2 = &Stub_MenuItem2;
    t.BeginTooltip = &Stub_BeginTooltip;
    t.EndTooltip = &Stub_EndTooltip;
    t.SetTooltipV = &Stub_SetTooltipV;
    t.BeginItemTooltip = &Stub_BeginItemTooltip;
    t.SetItemTooltipV = &Stub_SetItemTooltipV;
    t.BeginPopup = &Stub_BeginPopup;
    t.BeginPopupModal = &Stub_BeginPopupModal;
    t.EndPopup = &Stub_EndPopup;
    t.OpenPopup = &Stub_OpenPopup;
    t.OpenPopup2 = &Stub_OpenPopup2;
    t.OpenPopupOnItemClick = &Stub_OpenPopupOnItemClick;
    t.CloseCurrentPopup = &Stub_CloseCurrentPopup;
    t.BeginPopupContextItem = &Stub_BeginPopupContextItem;
    t.BeginPopupContextWindow = &Stub_BeginPopupContextWindow;
    t.BeginPopupContextVoid = &Stub_BeginPopupContextVoid;
    t.IsPopupOpen = &Stub_IsPopupOpen;
    t.BeginTable = &Stub_BeginTable;
    t.EndTable = &Stub_EndTable;
    t.TableNextRow = &Stub_TableNextRow;
    t.TableNextColumn = &Stub_TableNextColumn;
    t.TableSetColumnIndex = &Stub_TableSetColumnIndex;
    t.TableSetupColumn = &Stub_TableSetupColumn;
    t.TableSetupScrollFreeze = &Stub_TableSetupScrollFreeze;
    t.TableHeader = &Stub_TableHeader;
    t.TableHeadersRow = &Stub_TableHeadersRow;
    t.TableAngledHeadersRow = &Stub_TableAngledHeadersRow;
    t.TableGetSortSpecs = &Stub_TableGetSortSpecs;
    t.TableGetColumnCount = &Stub_TableGetColumnCount;
    t.TableGetColumnIndex = &Stub_TableGetColumnIndex;
    t.TableGetRowIndex = &Stub_TableGetRowIndex;
    t.TableGetColumnName = &Stub_TableGetColumnName;
    t.TableGetColumnFlags = &Stub_TableGetColumnFlags;
    t.TableSetColumnEnabled = &Stub_TableSetColumnEnabled;
    t.TableGetHoveredColumn = &Stub_TableGetHoveredColumn;
    t.TableSetBgColor = &Stub_TableSetBgColor;
    t.Columns = &Stub_Columns;
    t.NextColumn = &Stub_NextColumn;
    t.GetColumnIndex = &Stub_GetColumnIndex;
    t.GetColumnWidth = &Stub_GetColumnWidth;
    t.SetColumnWidth = &Stub_SetColumnWidth;
    t.GetColumnOffset = &Stub_GetColumnOffset;
    t.SetColumnOffset = &Stub_SetColumnOffset;
    t.GetColumnsCount = &Stub_GetColumnsCount;
    t.BeginTabBar = &Stub_BeginTabBar;
    t.EndTabBar = &Stub_EndTabBar;
    t.BeginTabItem = &Stub_BeginTabItem;
    t.EndTabItem = &Stub_EndTabItem;
    t.TabItemButton = &Stub_TabItemButton;
    t.SetTabItemClosed = &Stub_SetTabItemClosed;
    t.DockSpace = &Stub_DockSpace;
    t.SetNextWindowDockID = &Stub_SetNextWindowDockID;
    t.SetNextWindowClass = &Stub_SetNextWindowClass;
    t.GetWindowDockID = &Stub_GetWindowDockID;
    t.IsWindowDocked = &Stub_IsWindowDocked;
    t.BeginDragDropSource = &Stub_BeginDragDropSource;
    t.SetDragDropPayload = &Stub_SetDragDropPayload;
    t.EndDragDropSource = &Stub_EndDragDropSource;
    t.BeginDragDropTarget = &Stub_BeginDragDropTarget;
    t.AcceptDragDropPayload = &Stub_AcceptDragDropPayload;
    t.EndDragDropTarget = &Stub_EndDragDropTarget;
    t.GetDragDropPayload = &Stub_GetDragDropPayload;
    t.BeginDisabled = &Stub_BeginDisabled;
    t.EndDisabled = &Stub_EndDisabled;
    t.PushClipRect = &Stub_PushClipRect;
    t.PopClipRect = &Stub_PopClipRect;
    t.SetItemDefaultFocus = &Stub_SetItemDefaultFocus;
    t.SetKeyboardFocusHere = &Stub_SetKeyboardFocusHere;
    t.SetNavCursorVisible = &Stub_SetNavCursorVisible;
    t.SetNextItemAllowOverlap = &Stub_SetNextItemAllowOverlap;
    t.IsItemHovered = &Stub_IsItemHovered;
    t.IsItemActive = &Stub_IsItemActive;
    t.IsItemFocused = &Stub_IsItemFocused;
    t.IsItemClicked = &Stub_IsItemClicked;
    t.IsItemVisible = &Stub_IsItemVisible;
    t.IsItemEdited = &Stub_IsItemEdited;
    t.IsItemActivated = &Stub_IsItemActivated;
    t.IsItemDeactivated = &Stub_IsItemDeactivated;
    t.IsItemDeactivatedAfterEdit = &Stub_IsItemDeactivatedAfterEdit;
    t.IsItemToggledOpen = &Stub_IsItemToggledOpen;
    t.IsAnyItemHovered = &Stub_IsAnyItemHovered;
    t.IsAnyItemActive = &Stub_IsAnyItemActive;
    t.IsAnyItemFocused = &Stub_IsAnyItemFocused;
    t.GetItemID = &Stub_GetItemID;
    t.GetItemRectMin = &Stub_GetItemRectMin;
    t.GetItemRectMax = &Stub_GetItemRectMax;
    t.GetItemRectSize = &Stub_GetItemRectSize;
    t.GetBackgroundDrawList = &Stub_GetBackgroundDrawList;
    t.GetForegroundDrawList = &Stub_GetForegroundDrawList;
    t.IsRectVisible = &Stub_IsRectVisible;
    t.IsRectVisible2 = &Stub_IsRectVisible2;
    t.GetTime = &Stub_GetTime;
    t.GetFrameCount = &Stub_GetFrameCount;
    t.GetDrawListSharedData = &Stub_GetDrawListSharedData;
    t.GetStyleColorName = &Stub_GetStyleColorName;
    t.SetStateStorage = &Stub_SetStateStorage;
    t.GetStateStorage = &Stub_GetStateStorage;
    t.CalcTextSize = &Stub_CalcTextSize;
    t.ColorConvertU32ToFloat4 = &Stub_ColorConvertU32ToFloat4;
    t.ColorConvertFloat4ToU32 = &Stub_ColorConvertFloat4ToU32;
    t.ColorConvertRGBtoHSV = &Stub_ColorConvertRGBtoHSV;
    t.ColorConvertHSVtoRGB = &Stub_ColorConvertHSVtoRGB;
    t.IsKeyDown = &Stub_IsKeyDown;
    t.IsKeyPressed = &Stub_IsKeyPressed;
    t.IsKeyReleased = &Stub_IsKeyReleased;
    t.IsKeyChordPressed = &Stub_IsKeyChordPressed;
    t.GetKeyPressedAmount = &Stub_GetKeyPressedAmount;
    t.GetKeyName = &Stub_GetKeyName;
    t.SetNextFrameWantCaptureKeyboard = &Stub_SetNextFrameWantCaptureKeyboard;
    t.Shortcut = &Stub_Shortcut;
    t.SetNextItemShortcut = &Stub_SetNextItemShortcut;
    t.SetItemKeyOwner = &Stub_SetItemKeyOwner;
    t.IsMouseDown = &Stub_IsMouseDown;
    t.IsMouseClicked = &Stub_IsMouseClicked;
    t.IsMouseReleased = &Stub_IsMouseReleased;
    t.IsMouseDoubleClicked = &Stub_IsMouseDoubleClicked;
    t.IsMouseReleasedWithDelay = &Stub_IsMouseReleasedWithDelay;
    t.GetMouseClickedCount = &Stub_GetMouseClickedCount;
    t.IsMouseHoveringRect = &Stub_IsMouseHoveringRect;
    t.IsMousePosValid = &Stub_IsMousePosValid;
    t.IsAnyMouseDown = &Stub_IsAnyMouseDown;
    t.GetMousePos = &Stub_GetMousePos;
    t.GetMousePosOnOpeningCurrentPopup = &Stub_GetMousePosOnOpeningCurrentPopup;
    t.IsMouseDragging = &Stub_IsMouseDragging;
    t.GetMouseDragDelta = &Stub_GetMouseDragDelta;
    t.ResetMouseDragDelta = &Stub_ResetMouseDragDelta;
    t.GetMouseCursor = &Stub_GetMouseCursor;
    t.SetMouseCursor = &Stub_SetMouseCursor;
    t.SetNextFrameWantCaptureMouse = &Stub_SetNextFrameWantCaptureMouse;
    t.GetClipboardText = &Stub_GetClipboardText;
    t.SetClipboardText = &Stub_SetClipboardText;
    t.SetAllocatorFunctions = &Stub_SetAllocatorFunctions;
    t.GetAllocatorFunctions = &Stub_GetAllocatorFunctions;
    t.MemAlloc = &Stub_MemAlloc;
    t.MemFree = &Stub_MemFree;
    t.ImGuiStorage_GetInt = &Stub_ImGuiStorage_GetInt;
    t.ImGuiStorage_SetInt = &Stub_ImGuiStorage_SetInt;
    t.ImGuiStorage_GetBool = &Stub_ImGuiStorage_GetBool;
    t.ImGuiStorage_SetBool = &Stub_ImGuiStorage_SetBool;
    t.ImGuiStorage_GetFloat = &Stub_ImGuiStorage_GetFloat;
    t.ImGuiStorage_SetFloat = &Stub_ImGuiStorage_SetFloat;
    t.ImGuiStorage_GetVoidPtr = &Stub_ImGuiStorage_GetVoidPtr;
    t.ImGuiStorage_SetVoidPtr = &Stub_ImGuiStorage_SetVoidPtr;
    t.ImGuiStorage_GetIntRef = &Stub_ImGuiStorage_GetIntRef;
    t.ImGuiStorage_GetBoolRef = &Stub_ImGuiStorage_GetBoolRef;
    t.ImGuiStorage_GetFloatRef = &Stub_ImGuiStorage_GetFloatRef;
    t.ImGuiStorage_GetVoidPtrRef = &Stub_ImGuiStorage_GetVoidPtrRef;
    t.ImGuiStorage_BuildSortByKey = &Stub_ImGuiStorage_BuildSortByKey;
    t.ImGuiStorage_SetAllInt = &Stub_ImGuiStorage_SetAllInt;
    t.ConstructImGuiListClipper = &Stub_ConstructImGuiListClipper;
    t.DestructImGuiListClipper = &Stub_DestructImGuiListClipper;
    t.ImGuiListClipper_Begin = &Stub_ImGuiListClipper_Begin;
    t.ImGuiListClipper_End = &Stub_ImGuiListClipper_End;
    t.ImGuiListClipper_Step = &Stub_ImGuiListClipper_Step;
    t.ImGuiListClipper_IncludeItemsByIndex = &Stub_ImGuiListClipper_IncludeItemsByIndex;
    t.ImGuiListClipper_SeekCursorForItem = &Stub_ImGuiListClipper_SeekCursorForItem;
    t.ConstructImDrawList = &Stub_ConstructImDrawList;
    t.DestructImDrawList = &Stub_DestructImDrawList;
    t.ImDrawList_PushClipRect = &Stub_ImDrawList_PushClipRect;
    t.ImDrawList_PushClipRectFullScreen = &Stub_ImDrawList_PushClipRectFullScreen;
    t.ImDrawList_PopClipRect = &Stub_ImDrawList_PopClipRect;
    t.ImDrawList_PushTexture = &Stub_ImDrawList_PushTexture;
    t.ImDrawList_PopTexture = &Stub_ImDrawList_PopTexture;
    t.ImDrawList_AddLine = &Stub_ImDrawList_AddLine;
    t.ImDrawList_AddRect = &Stub_ImDrawList_AddRect;
    t.ImDrawList_AddRectFilled = &Stub_ImDrawList_AddRectFilled;
    t.ImDrawList_AddRectFilledMultiColor = &Stub_ImDrawList_AddRectFilledMultiColor;
    t.ImDrawList_AddQuad = &Stub_ImDrawList_AddQuad;
    t.ImDrawList_AddQuadFilled = &Stub_ImDrawList_AddQuadFilled;
    t.ImDrawList_AddTriangle = &Stub_ImDrawList_AddTriangle;
    t.ImDrawList_AddTriangleFilled = &Stub_ImDrawList_AddTriangleFilled;
    t.ImDrawList_AddCircle = &Stub_ImDrawList_AddCircle;
    t.ImDrawList_AddCircleFilled = &Stub_ImDrawList_AddCircleFilled;
    t.ImDrawList_AddNgon = &Stub_ImDrawList_AddNgon;
    t.ImDrawList_AddNgonFilled = &Stub_ImDrawList_AddNgonFilled;
    t.ImDrawList_AddEllipse = &Stub_ImDrawList_AddEllipse;
    t.ImDrawList_AddEllipseFilled = &Stub_ImDrawList_AddEllipseFilled;
    t.ImDrawList_AddText = &Stub_ImDrawList_AddText;
    t.ImDrawList_AddText2 = &Stub_ImDrawList_AddText2;
    t.ImDrawList_AddBezierCubic = &Stub_ImDrawList_AddBezierCubic;
    t.ImDrawList_AddBezierQuadratic = &Stub_ImDrawList_AddBezierQuadratic;
    t.ImDrawList_AddPolyline = &Stub_ImDrawList_AddPolyline;
    t.ImDrawList_AddConvexPolyFilled = &Stub_ImDrawList_AddConvexPolyFilled;
    t.ImDrawList_AddConcavePolyFilled = &Stub_ImDrawList_AddConcavePolyFilled;
    t.ImDrawList_AddImage = &Stub_ImDrawList_AddImage;
    t.ImDrawList_AddImageQuad = &Stub_ImDrawList_AddImageQuad;
    t.ImDrawList_AddImageRounded = &Stub_ImDrawList_AddImageRounded;
    t.ImDrawList_PathArcTo = &Stub_ImDrawList_PathArcTo;
    t.ImDrawList_PathArcToFast = &Stub_ImDrawList_PathArcToFast;
    t.ImDrawList_PathEllipticalArcTo = &Stub_ImDrawList_PathEllipticalArcTo;
    t.ImDrawList_PathBezierCubicCurveTo = &Stub_ImDrawList_PathBezierCubicCurveTo;
    t.ImDrawList_PathBezierQuadraticCurveTo = &Stub_ImDrawList_PathBezierQuadraticCurveTo;
    t.ImDrawList_PathRect = &Stub_ImDrawList_PathRect;
    t.ImDrawList_AddCallback = &Stub_ImDrawList_AddCallback;
    t.ImDrawList_AddDrawCmd = &Stub_ImDrawList_AddDrawCmd;
    t.ImDrawList_CloneOutput = &Stub_ImDrawList_CloneOutput;
    t.ImDrawList_PrimReserve = &Stub_ImDrawList_PrimReserve;
    t.ImDrawList_PrimUnreserve = &Stub_ImDrawList_PrimUnreserve;
    t.ImDrawList_PrimRect = &Stub_ImDrawList_PrimRect;
    t.ImDrawList_PrimRectUV = &Stub_ImDrawList_PrimRectUV;
    t.ImDrawList_PrimQuadUV = &Stub_ImDrawList_PrimQuadUV;
    t.ConstructImFont = &Stub_ConstructImFont;
    t.DestructImFont = &Stub_DestructImFont;
    t.ImFont_IsGlyphInFont = &Stub_ImFont_IsGlyphInFont;
}
inline constexpr int kImGuiFunctions = 421;
#pragma warning(pop)
