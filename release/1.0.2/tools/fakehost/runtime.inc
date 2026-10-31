    uint64_t get_native() const override { Trap(0, "get_native"); return {}; }
    void get_private_data(const uint8_t guid[16], uint64_t *data) const override { (void)guid; (void)data; Trap(1, "get_private_data"); }
    void set_private_data(const uint8_t guid[16], const uint64_t data) override { (void)guid; (void)data; Trap(2, "set_private_data"); }
    device * get_device() override { return nullptr; }
    void * get_hwnd() const override { Trap(4, "get_hwnd"); return {}; }
    resource get_back_buffer(uint32_t index) override { (void)index; Trap(5, "get_back_buffer"); return {}; }
    uint32_t get_back_buffer_count() const override { Trap(6, "get_back_buffer_count"); return {}; }
    uint32_t get_current_back_buffer_index() const override { Trap(7, "get_current_back_buffer_index"); return {}; }
    command_queue * get_command_queue() override { Trap(8, "get_command_queue"); return {}; }
    void render_effects(command_list *cmd_list, resource_view rtv, resource_view rtv_srgb) override { (void)cmd_list; (void)rtv; (void)rtv_srgb; Trap(9, "render_effects"); }
    bool capture_screenshot(void *pixels) override { (void)pixels; Trap(10, "capture_screenshot"); return {}; }
    void get_screenshot_width_and_height(uint32_t *out_width, uint32_t *out_height) const override { (void)out_width; (void)out_height; Trap(11, "get_screenshot_width_and_height"); }
    bool is_key_down(uint32_t keycode) const override { return keys.Down(keycode); }
    bool is_key_pressed(uint32_t keycode) const override { return keys.Pressed(keycode); }
    bool is_key_released(uint32_t keycode) const override { (void)keycode; return false; }
    bool is_mouse_button_down(uint32_t button) const override { (void)button; return false; }
    bool is_mouse_button_pressed(uint32_t button) const override { (void)button; return false; }
    bool is_mouse_button_released(uint32_t button) const override { (void)button; return false; }
    void get_mouse_cursor_position(uint32_t *out_x, uint32_t *out_y, int16_t *out_wheel_delta) const override { if (out_x) *out_x = 0; if (out_y) *out_y = 0; if (out_wheel_delta) *out_wheel_delta = 0; }
    void enumerate_uniform_variables(const char *effect_name, void(*callback)(effect_runtime *runtime, effect_uniform_variable variable, void *user_data), void *user_data) override { (void)effect_name; (void)callback; (void)user_data; Trap(19, "enumerate_uniform_variables"); }
    effect_uniform_variable find_uniform_variable(const char *effect_name, const char *variable_name) const override { (void)effect_name; (void)variable_name; Trap(20, "find_uniform_variable"); return {}; }
    void get_uniform_variable_type(effect_uniform_variable variable, format *out_base_type, uint32_t *out_rows, uint32_t *out_columns, uint32_t *out_array_length) const override { (void)variable; (void)out_base_type; (void)out_rows; (void)out_columns; (void)out_array_length; Trap(21, "get_uniform_variable_type"); }
    void get_uniform_variable_name(effect_uniform_variable variable, char *name, size_t *name_size) const override { (void)variable; (void)name; (void)name_size; Trap(22, "get_uniform_variable_name"); }
    bool get_annotation_bool_from_uniform_variable(effect_uniform_variable variable, const char *name, bool *values, size_t count, size_t array_index) const override { (void)variable; (void)name; (void)values; (void)count; (void)array_index; Trap(23, "get_annotation_bool_from_uniform_variable"); return {}; }
    bool get_annotation_float_from_uniform_variable(effect_uniform_variable variable, const char *name, float *values, size_t count, size_t array_index) const override { (void)variable; (void)name; (void)values; (void)count; (void)array_index; Trap(24, "get_annotation_float_from_uniform_variable"); return {}; }
    bool get_annotation_int_from_uniform_variable(effect_uniform_variable variable, const char *name, int32_t *values, size_t count, size_t array_index) const override { (void)variable; (void)name; (void)values; (void)count; (void)array_index; Trap(25, "get_annotation_int_from_uniform_variable"); return {}; }
    bool get_annotation_uint_from_uniform_variable(effect_uniform_variable variable, const char *name, uint32_t *values, size_t count, size_t array_index) const override { (void)variable; (void)name; (void)values; (void)count; (void)array_index; Trap(26, "get_annotation_uint_from_uniform_variable"); return {}; }
    bool get_annotation_string_from_uniform_variable(effect_uniform_variable variable, const char *name, char *value, size_t *value_size) const override { (void)variable; (void)name; (void)value; (void)value_size; Trap(27, "get_annotation_string_from_uniform_variable"); return {}; }
    void get_uniform_value_bool(effect_uniform_variable variable, bool *values, size_t count, size_t array_index) const override { (void)variable; (void)values; (void)count; (void)array_index; Trap(28, "get_uniform_value_bool"); }
    void get_uniform_value_float(effect_uniform_variable variable, float *values, size_t count, size_t array_index) const override { (void)variable; (void)values; (void)count; (void)array_index; Trap(29, "get_uniform_value_float"); }
    void get_uniform_value_int(effect_uniform_variable variable, int32_t *values, size_t count, size_t array_index) const override { (void)variable; (void)values; (void)count; (void)array_index; Trap(30, "get_uniform_value_int"); }
    void get_uniform_value_uint(effect_uniform_variable variable, uint32_t *values, size_t count, size_t array_index) const override { (void)variable; (void)values; (void)count; (void)array_index; Trap(31, "get_uniform_value_uint"); }
    void set_uniform_value_bool(effect_uniform_variable variable, const bool *values, size_t count, size_t array_index) override { (void)variable; (void)values; (void)count; (void)array_index; Trap(32, "set_uniform_value_bool"); }
    void set_uniform_value_float(effect_uniform_variable variable, const float *values, size_t count, size_t array_index) override { (void)variable; (void)values; (void)count; (void)array_index; Trap(33, "set_uniform_value_float"); }
    void set_uniform_value_int(effect_uniform_variable variable, const int32_t *values, size_t count, size_t array_index) override { (void)variable; (void)values; (void)count; (void)array_index; Trap(34, "set_uniform_value_int"); }
    void set_uniform_value_uint(effect_uniform_variable variable, const uint32_t *values, size_t count, size_t array_index) override { (void)variable; (void)values; (void)count; (void)array_index; Trap(35, "set_uniform_value_uint"); }
    void enumerate_texture_variables(const char *effect_name, void(*callback)(effect_runtime *runtime, effect_texture_variable variable, void *user_data), void *user_data) override { (void)effect_name; (void)callback; (void)user_data; Trap(36, "enumerate_texture_variables"); }
    effect_texture_variable find_texture_variable(const char *effect_name, const char *variable_name) const override { (void)effect_name; (void)variable_name; Trap(37, "find_texture_variable"); return {}; }
    void get_texture_variable_name(effect_texture_variable variable, char *name, size_t *name_size) const override { (void)variable; (void)name; (void)name_size; Trap(38, "get_texture_variable_name"); }
    bool get_annotation_bool_from_texture_variable(effect_texture_variable variable, const char *name, bool *values, size_t count, size_t array_index) const override { (void)variable; (void)name; (void)values; (void)count; (void)array_index; Trap(39, "get_annotation_bool_from_texture_variable"); return {}; }
    bool get_annotation_float_from_texture_variable(effect_texture_variable variable, const char *name, float *values, size_t count, size_t array_index) const override { (void)variable; (void)name; (void)values; (void)count; (void)array_index; Trap(40, "get_annotation_float_from_texture_variable"); return {}; }
    bool get_annotation_int_from_texture_variable(effect_texture_variable variable, const char *name, int32_t *values, size_t count, size_t array_index) const override { (void)variable; (void)name; (void)values; (void)count; (void)array_index; Trap(41, "get_annotation_int_from_texture_variable"); return {}; }
    bool get_annotation_uint_from_texture_variable(effect_texture_variable variable, const char *name, uint32_t *values, size_t count, size_t array_index) const override { (void)variable; (void)name; (void)values; (void)count; (void)array_index; Trap(42, "get_annotation_uint_from_texture_variable"); return {}; }
    bool get_annotation_string_from_texture_variable(effect_texture_variable variable, const char *name, char *value, size_t *value_size) const override { (void)variable; (void)name; (void)value; (void)value_size; Trap(43, "get_annotation_string_from_texture_variable"); return {}; }
    void update_texture(effect_texture_variable variable, const uint32_t width, const uint32_t height, const void *pixels) override { (void)variable; (void)width; (void)height; (void)pixels; Trap(44, "update_texture"); }
    void get_texture_binding(effect_texture_variable variable, resource_view *out_srv, resource_view *out_srv_srgb) const override { (void)variable; (void)out_srv; (void)out_srv_srgb; Trap(45, "get_texture_binding"); }
    void update_texture_bindings(const char *semantic, resource_view srv, resource_view srv_srgb) override { (void)semantic; (void)srv; (void)srv_srgb; Trap(46, "update_texture_bindings"); }
    void enumerate_techniques(const char *effect_name, void(*callback)(effect_runtime *runtime, effect_technique technique, void *user_data), void *user_data) override { (void)effect_name; (void)callback; (void)user_data; Trap(47, "enumerate_techniques"); }
    effect_technique find_technique(const char *effect_name, const char *technique_name) override { (void)effect_name; (void)technique_name; Trap(48, "find_technique"); return {}; }
    void get_technique_name(effect_technique technique, char *name, size_t *name_size) const override { (void)technique; (void)name; (void)name_size; Trap(49, "get_technique_name"); }
    bool get_annotation_bool_from_technique(effect_technique technique, const char *name, bool *values, size_t count, size_t array_index) const override { (void)technique; (void)name; (void)values; (void)count; (void)array_index; Trap(50, "get_annotation_bool_from_technique"); return {}; }
    bool get_annotation_float_from_technique(effect_technique technique, const char *name, float *values, size_t count, size_t array_index) const override { (void)technique; (void)name; (void)values; (void)count; (void)array_index; Trap(51, "get_annotation_float_from_technique"); return {}; }
    bool get_annotation_int_from_technique(effect_technique technique, const char *name, int32_t *values, size_t count, size_t array_index) const override { (void)technique; (void)name; (void)values; (void)count; (void)array_index; Trap(52, "get_annotation_int_from_technique"); return {}; }
    bool get_annotation_uint_from_technique(effect_technique technique, const char *name, uint32_t *values, size_t count, size_t array_index) const override { (void)technique; (void)name; (void)values; (void)count; (void)array_index; Trap(53, "get_annotation_uint_from_technique"); return {}; }
    bool get_annotation_string_from_technique(effect_technique technique, const char *name, char *value, size_t *value_size) const override { (void)technique; (void)name; (void)value; (void)value_size; Trap(54, "get_annotation_string_from_technique"); return {}; }
    bool get_technique_state(effect_technique technique) const override { (void)technique; Trap(55, "get_technique_state"); return {}; }
    void set_technique_state(effect_technique technique, bool enabled) override { (void)technique; (void)enabled; Trap(56, "set_technique_state"); }
    bool get_preprocessor_definition(const char *name, char *value, size_t *value_size) const override { (void)name; (void)value; (void)value_size; Trap(57, "get_preprocessor_definition"); return {}; }
    void set_preprocessor_definition(const char *name, const char *value) override { (void)name; (void)value; Trap(58, "set_preprocessor_definition"); }
    void render_technique(effect_technique technique, command_list *cmd_list, resource_view rtv, resource_view rtv_srgb) override { (void)technique; (void)cmd_list; (void)rtv; (void)rtv_srgb; Trap(59, "render_technique"); }
    bool get_effects_state() const override { Trap(60, "get_effects_state"); return {}; }
    void set_effects_state(bool enabled) override { (void)enabled; Trap(61, "set_effects_state"); }
    void get_current_preset_path(char *path, size_t *path_size) const override { (void)path; (void)path_size; Trap(62, "get_current_preset_path"); }
    void set_current_preset_path(const char *path) override { (void)path; Trap(63, "set_current_preset_path"); }
    void reorder_techniques(size_t count, const effect_technique *techniques) override { (void)count; (void)techniques; Trap(64, "reorder_techniques"); }
    void block_input_next_frame() override { Trap(65, "block_input_next_frame"); }
    uint32_t last_key_pressed() const override { Trap(66, "last_key_pressed"); return {}; }
    uint32_t last_key_released() const override { Trap(67, "last_key_released"); return {}; }
    void get_uniform_variable_effect_name(effect_uniform_variable variable, char *effect_name, size_t *effect_name_size) const override { (void)variable; (void)effect_name; (void)effect_name_size; Trap(68, "get_uniform_variable_effect_name"); }
    void get_texture_variable_effect_name(effect_texture_variable variable, char *effect_name, size_t *effect_name_size) const override { (void)variable; (void)effect_name; (void)effect_name_size; Trap(69, "get_texture_variable_effect_name"); }
    void get_technique_effect_name(effect_technique technique, char *effect_name, size_t *effect_name_size) const override { (void)technique; (void)effect_name; (void)effect_name_size; Trap(70, "get_technique_effect_name"); }
    void save_current_preset() const override { Trap(71, "save_current_preset"); }
    bool get_preprocessor_definition_for_effect(const char *effect_name, const char *name, char *value, size_t *value_size) const override { (void)effect_name; (void)name; (void)value; (void)value_size; Trap(72, "get_preprocessor_definition_for_effect"); return {}; }
    void set_preprocessor_definition_for_effect(const char *effect_name, const char *name, const char *value) override { (void)effect_name; (void)name; (void)value; Trap(73, "set_preprocessor_definition_for_effect"); }
    bool open_overlay(bool open, input_source source) override { (void)open; (void)source; Trap(74, "open_overlay"); return {}; }
    void set_color_space(color_space color_space) override { (void)color_space; Trap(75, "set_color_space"); }
    void reset_uniform_value(effect_uniform_variable variable) override { (void)variable; Trap(76, "reset_uniform_value"); }
    void reload_effect_next_frame(const char *effect_name) override { (void)effect_name; Trap(77, "reload_effect_next_frame"); }
    void export_current_preset(const char *path) const override { (void)path; Trap(78, "export_current_preset"); }
    void save_screenshot(const char *postfix) override { (void)postfix; Trap(79, "save_screenshot"); }
