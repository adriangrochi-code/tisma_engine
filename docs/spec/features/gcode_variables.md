# Variables de los G-code de las máquinas

**Qué es:** la lista de la regla 9 de `gcode_macros.md`: cada variable que usan los G-code con macros del paquete de
perfiles (bloque `gcode`, `profile_pack_format.md`), por dialecto, con su tipo, en qué G-code aparece (y en cuántas
variantes) y su equivalente con nombre de PrusaSlicer, que es la clave pública cuyo valor la llena. La contó la
sesión del paquete en sus datos (`docs/GCODE_VARIABLES.md` del paquete, 05-11-2026). Sin equivalente, su significado
sale de la documentación pública de cada programa; si no hay, se avisa en el registro como dice la regla 9.
«Propia de la macro»: la asigna el mismo G-code.

## `prusaslicer`

104 variables.

| Variable | Tipo | Dónde | Equivalente |
|---|---|---|---|
| `first_layer_temperature` | lista de enteros | `start`, `toolchange` (1033) | `first_layer_temperature` |
| `first_layer_bed_temperature` | lista de enteros | `start` (1023) | `first_layer_bed_temperature` |
| `layer_z` | número | `layer_change`, `before_layer_change`, `end`, `toolchange` (960) | `layer_z` |
| `max_layer_z` | número | `end` (600) | `max_layer_z` |
| `max_print_height` | número | `end`, `start` (549) | `max_print_height` |
| `z_offset` | número | `end` (521) | `z_offset` |
| `print_bed_max` | lista de números | `start`, `end` (411) | `print_bed_max` |
| `travel_speed` | número | `start`, `end`, `toolchange` (360) | `travel_speed` |
| `nozzle_diameter` | lista de números | `start` (291) | `nozzle_diameter` |
| `idle_temperature` | lista de enteros | `start`, `toolchange` (289) | `idle_temperature` |
| `layer_num` | entero | `toolchange`, `layer_change`, `before_layer_change`, `end` (278) | `layer_num` |
| `first_layer_print_min` | lista de números | `start` (266) | `first_layer_print_min` |
| `first_layer_print_max` | lista de números | `start` (242) | `first_layer_print_max` |
| `filament_type` | lista de textos | `start`, `toolchange` (219) | `filament_type` |
| `initial_tool` | entero | `start`, `toolchange` (196) | `initial_tool` |
| `filament_abrasive` | lista de sí/no | `start` (155) | `filament_abrasive` |
| `nozzle_high_flow` | lista de sí/no | `start` (155) | `nozzle_high_flow` |
| `filament_notes` | lista de textos | `start` (154) | `filament_notes` |
| `chamber_temperature` | lista de enteros | `start` (136) | `chamber_temperature` |
| `temperature` | lista de enteros | `start`, `toolchange`, `before_layer_change` (135) | `temperature` |
| `layer_height` | número | `start` (133) | `layer_height` |
| `chamber_minimal_temperature` | lista de enteros | `start` (116) | `chamber_minimal_temperature` |
| `next_extruder` | entero | `toolchange` (112) | `next_extruder` |
| `retract_length` | lista de números | `start` (105) | `retract_length` |
| `printer_model` | texto | `start` (98) | `printer_model` |
| `is_extruder_used` | lista de sí/no | `start`, `end`, `layer_change` (97) | `is_extruder_used` |
| `first_layer_print_min_0` |  | `end`, `start` (84) |  |
| `first_layer_print_min_1` |  | `end`, `start` (84) |  |
| `machine_max_acceleration_extruding` | lista de números | `start` (81) | `machine_max_acceleration_extruding` |
| `machine_max_acceleration_retracting` | lista de números | `start` (81) | `machine_max_acceleration_retracting` |
| `machine_max_acceleration_travel` | lista de números | `start` (80) | `machine_max_acceleration_travel` |
| `initial_extruder` | entero | `start` (79) | `initial_extruder` |
| `first_layer_height` | número o porcentaje | `start`, `toolchange` (78) | `first_layer_height` |
| `bed_temperature` | lista de enteros | `toolchange`, `start`, `layer_change` (76) | `bed_temperature` |
| `retract_length_toolchange` | lista de números | `start`, `toolchange` (75) | `retract_length_toolchange` |
| `standby_temperature_delta` | entero | `start`, `toolchange` (75) | `standby_temperature_delta` |
| `machine_max_feedrate_x` | lista de números | `start`, `end` (73) | `machine_max_feedrate_x` |
| `machine_max_feedrate_z` | lista de números | `start`, `end` (73) | `machine_max_feedrate_z` |
| `first_layer_print_max_0` |  | `end` (72) |  |
| `first_layer_print_max_1` |  | `end` (72) |  |
| `machine_max_acceleration_e` | lista de números | `start` (72) | `machine_max_acceleration_e` |
| `machine_max_acceleration_x` | lista de números | `start` (72) | `machine_max_acceleration_x` |
| `machine_max_acceleration_y` | lista de números | `start` (72) | `machine_max_acceleration_y` |
| `machine_max_acceleration_z` | lista de números | `start` (72) | `machine_max_acceleration_z` |
| `machine_max_feedrate_e` | lista de números | `start` (72) | `machine_max_feedrate_e` |
| `machine_max_feedrate_y` | lista de números | `start` (72) | `machine_max_feedrate_y` |
| `machine_max_jerk_e` | lista de números | `start` (72) | `machine_max_jerk_e` |
| `machine_max_jerk_x` | lista de números | `start` (72) | `machine_max_jerk_x` |
| `machine_max_jerk_y` | lista de números | `start` (72) | `machine_max_jerk_y` |
| `machine_max_jerk_z` | lista de números | `start` (72) | `machine_max_jerk_z` |
| `max_fan_speed` | lista de enteros | `start` (72) | `max_fan_speed` |
| `physical_printer_preset` | texto | `start` (72) | `physical_printer_preset` |
| `print_preset` | texto | `start` (72) | `print_preset` |
| `printer_preset` | texto | `start` (72) | `printer_preset` |
| `current_extruder` | entero | `toolchange`, `start` (66) | `current_extruder` |
| `extruded_weight_total` | número | `before_layer_change`, `layer_change` (65) | `extruded_weight_total` |
| `spiral_vase` | sí/no | `layer_change` (65) | `spiral_vase` |
| `has_wipe_tower` | sí/no | `start`, `toolchange`, `end` (60) | `has_wipe_tower` |
| `total_layer_count` | entero | `start`, `end` (58) | `total_layer_count` |
| `print_bed_min` | lista de números | `start` (50) | `print_bed_min` |
| `default_acceleration` | número | `toolchange` (40) | `default_acceleration` |
| `disable_fan_first_layers` | lista de enteros | `toolchange` (40) | `disable_fan_first_layers` |
| `first_layer_acceleration` | número | `toolchange` (40) | `first_layer_acceleration` |
| `min_fan_speed` | lista de enteros | `toolchange` (40) | `min_fan_speed` |
| `total_toolchanges` | entero | `start`, `toolchange` (40) | `total_toolchanges` |
| `retract_speed` | lista de números | `toolchange`, `start` (37) | `retract_speed` |
| `wipe_tower_x` |  | `toolchange` (36) |  |
| `wipe_tower_y` |  | `toolchange` (36) |  |
| `previous_extruder` | entero | `toolchange` (35) | `previous_extruder` |
| `e_retracted` | lista de números | `start` (32) | `e_retracted` |
| `filament_multitool_ramming` | lista de sí/no | `start` (32) | `filament_multitool_ramming` |
| `max_speed_toolchange` | propia de la macro | `toolchange` (32) |  |
| `num_extruders` | entero | `start` (32) | `num_extruders` |
| `position` | lista de números | `toolchange` (32) | `position` |
| `speed_toolchange` | propia de la macro | `toolchange` (32) |  |
| `then` |  | `toolchange` (32) |  |
| `wait_for_extruder_temp` | propia de la macro | `toolchange` (32) |  |
| `zhop` | número | `start` (32) | `zhop` |
| `print_settings_id` | texto | `end`, `start` (29) | `print_settings_id` |
| `extra_loading_move` | número | `start` (21) | `extra_loading_move` |
| `parking_pos_retraction` | número | `start` (21) | `parking_pos_retraction` |
| `has_single_extruder_multi_material_priming` | sí/no | `start` (17) | `has_single_extruder_multi_material_priming` |
| `Checks` |  | `start` (12) |  |
| `Config` |  | `start` (12) |  |
| `Macros` |  | `start` (12) |  |
| `System` |  | `start` (12) |  |
| `directories` |  | `start` (12) |  |
| `filament_check` |  | `start` (12) |  |
| `filament_settings_id` | lista de textos | `start` (12) | `filament_settings_id` |
| `move` |  | `start` (12) |  |
| `nozzle_check` |  | `start` (12) |  |
| `first_layer_temperature_` |  | `start` (9) |  |
| `print_bed_size` | lista de números | `start` (5) | `print_bed_size` |
| `filament_max_volumetric_speed` | lista de números | `toolchange` (4) | `filament_max_volumetric_speed` |
| `load_heightmap` |  | `start` (4) |  |
| `printer_settings_id` | texto | `start` (4) | `printer_settings_id` |
| `start_filament_gcode` | lista de textos | `start` (4) | `start_filament_gcode` |
| `travel_acceleration` | número | `toolchange` (4) | `travel_acceleration` |
| `day` | entero | `start` (2) | `day` |
| `extruded_volume` | lista de números | `start` (2) | `extruded_volume` |
| `extrusion_width` | número o porcentaje | `start` (2) | `extrusion_width` |
| `filament_diameter` | lista de números | `start` (2) | `filament_diameter` |
| `month` | entero | `start` (2) | `month` |
| `year` | entero | `start` (2) | `year` |
## `orcaslicer`

250 variables.

| Variable | Tipo | Dónde | Equivalente |
|---|---|---|---|
| `layer_z` | número | `before_layer_change`, `layer_change`, `toolchange`, `timelapse`, `end`, `toolchange_gcode` (735) | `layer_z` |
| `nozzle_temperature_initial_layer` | lista de enteros | `start`, `toolchange`, `toolchange_gcode`, `before_layer_change`, `end` (572) | `first_layer_temperature` |
| `bed_temperature_initial_layer_single` | entero | `start`, `layer_change`, `before_layer_change` (501) | `first_layer_bed_temperature` |
| `layer_num` | entero | `layer_change`, `before_layer_change`, `toolchange`, `wrapping_detection_gcode`, `timelapse`, `toolchange_gcode`, `end` (281) | `layer_num` |
| `first_layer_print_min` | lista de números | `start`, `layer_change`, `before_layer_change` (268) | `first_layer_print_min` |
| `first_layer_temperature` | lista de enteros | `start`, `toolchange` (245) | `first_layer_temperature` |
| `max_layer_z` | número | `end`, `toolchange`, `timelapse` (245) | `max_layer_z` |
| `total_layer_count` | entero | `start`, `layer_change` (214) | `total_layer_count` |
| `first_layer_print_max` | lista de números | `start`, `layer_change`, `before_layer_change` (194) | `first_layer_print_max` |
| `first_layer_bed_temperature` | lista de enteros | `start` (179) | `first_layer_bed_temperature` |
| `next_extruder` | entero | `toolchange`, `toolchange_gcode`, `filament_swap_gcode` (150) | `next_extruder` |
| `print_bed_max` | lista de números | `start`, `end`, `toolchange`, `layer_change` (140) | `print_bed_max` |
| `chamber_temperature` | lista de enteros | `start`, `end` (139) | `chamber_temperature` |
| `initial_no_support_extruder` | entero | `start`, `toolchange` (128) | `initial_extruder` |
| `filament_type` | lista de textos | `start`, `toolchange`, `layer_change`, `before_layer_change`, `end` (118) | `filament_type` |
| `curr_bed_type` | opción | `start`, `custom` (117) | `curr_bed_type` |
| `initial_tool` | entero | `start`, `toolchange` (112) | `initial_tool` |
| `nozzle_temperature` | lista de enteros | `start`, `toolchange`, `before_layer_change`, `end`, `toolchange_gcode` (105) | `temperature` |
| `bed_temperature_initial_layer` | lista de enteros | `start` (102) | `first_layer_bed_temperature` |
| `z_after_toolchange` | número | `toolchange` (102) |  |
| `printable_height` | número | `end`, `toolchange` (96) | `max_print_height` |
| `initial_extruder` | entero | `start`, `toolchange` (93) | `initial_extruder` |
| `new_filament_temp` | entero | `toolchange` (91) |  |
| `initial_layer_print_height` | número | `toolchange`, `start`, `before_layer_change`, `toolchange_gcode` (84) | `first_layer_height` |
| `max_print_height` | entero | `end`, `toolchange`, `layer_change` (84) | `max_print_height` |
| `old_filament_temp` | entero | `toolchange` (78) |  |
| `filament_max_volumetric_speed` | lista de números | `start`, `toolchange` (74) | `filament_max_volumetric_speed` |
| `z_offset` | número | `end`, `toolchange`, `start` (70) | `z_offset` |
| `first_layer_print_size` | lista de números | `start` (68) | `first_layer_print_size` |
| `travel_speed` | lista de números | `end`, `start`, `toolchange` (64) | `travel_speed` |
| `bed_temperature` | lista de enteros | `start`, `end`, `before_layer_change`, `toolchange` (63) | `bed_temperature` |
| `initial_layer_acceleration` | lista de números | `toolchange` (62) |  |
| `previous_extruder` | entero | `toolchange` (60) | `previous_extruder` |
| `x_after_toolchange` | número | `toolchange` (60) |  |
| `y_after_toolchange` | número | `toolchange` (60) |  |
| `flush_length` | número | `toolchange` (59) |  |
| `new_filament_e_feedrate` | entero | `toolchange` (59) |  |
| `old_filament_e_feedrate` | entero | `toolchange` (59) |  |
| `toolchange_count` | entero | `toolchange` (58) |  |
| `flush_temperatures` | lista de enteros | `toolchange`, `start` (56) |  |
| `flush_volumetric_speeds` | lista de números | `toolchange`, `start`, `end` (56) |  |
| `retraction_distances_when_cut` | lista de números | `toolchange`, `start` (56) |  |
| `timelapse_type` | opción | `timelapse`, `layer_change` (56) |  |
| `outer_wall_volumetric_speed` | número | `start`, `toolchange` (54) |  |
| `spiral_mode` | sí/no | `timelapse`, `wrapping_detection_gcode`, `layer_change`, `end` (53) | `spiral_vase` |
| `default_acceleration` | lista de números | `toolchange`, `start` (50) | `default_acceleration` |
| `flush_length_1` |  | `toolchange` (48) |  |
| `flush_length_2` |  | `toolchange` (48) |  |
| `flush_length_3` |  | `toolchange` (48) |  |
| `flush_length_4` |  | `toolchange` (48) |  |
| `new_retract_length_toolchange` | número | `toolchange` (48) |  |
| `travel_point_1_x` | número | `toolchange` (48) |  |
| `travel_point_1_y` | número | `toolchange` (48) |  |
| `travel_point_2_x` | número | `toolchange` (48) |  |
| `travel_point_2_y` | número | `toolchange` (48) |  |
| `travel_point_3_x` | número | `toolchange` (48) |  |
| `travel_point_3_y` | número | `toolchange` (48) |  |
| `long_retractions_when_cut` | lista de sí/no | `toolchange` (44) |  |
| `temperature` | lista de enteros | `start`, `toolchange` (42) | `temperature` |
| `old_retract_length_toolchange` | número | `toolchange` (40) |  |
| `printer_model` | texto | `start` (37) | `printer_model` |
| `current_extruder` | entero | `toolchange`, `end`, `start`, `toolchange_gcode` (36) | `current_extruder` |
| `nozzle_temperature_range_high` | lista de enteros | `toolchange`, `start` (35) |  |
| `nozzle_diameter` | lista de números | `start`, `toolchange` (34) | `nozzle_diameter` |
| `print_sequence` | opción | `end`, `timelapse`, `toolchange` (34) |  |
| `filament_map` | lista de enteros | `toolchange`, `start`, `end` (32) |  |
| `long_retraction_when_cut` | sí/no | `toolchange`, `end` (32) |  |
| `print_time_sec` | texto | `file_start_gcode` (32) |  |
| `retraction_distance_when_cut` | número | `toolchange`, `end` (32) |  |
| `adaptive_bed_mesh_max` | lista de números | `start` (30) | `first_layer_print_max` |
| `adaptive_bed_mesh_min` | lista de números | `start` (30) | `first_layer_print_min` |
| `bed_mesh_probe_count` | lista de enteros | `start` (28) |  |
| `filament_cooling_before_tower` | lista de números | `toolchange` (28) |  |
| `outer_wall_acceleration` | lista de números | `start` (28) |  |
| `overall_chamber_temperature` | entero | `start`, `layer_change`, `toolchange` (28) |  |
| `print_time_day` | texto | `file_start_gcode` (28) |  |
| `print_time_hour` | texto | `file_start_gcode` (28) |  |
| `print_time_minute` | texto | `file_start_gcode` (28) |  |
| `retraction_length` | lista de números | `start`, `toolchange`, `layer_change` (28) | `retract_length` |
| `is_extruder_used` | lista de sí/no | `start`, `layer_change`, `end` (27) | `is_extruder_used` |
| `standby_temperature_delta` | entero | `start` (26) | `standby_temperature_delta` |
| `print_bed_min` | lista de números | `start` (25) | `print_bed_min` |
| `current_filament_id` | entero | `toolchange`, `layer_change`, `end` (24) |  |
| `current_nozzle_id` | entero | `toolchange`, `layer_change` (24) |  |
| `initial_no_support_filament_id` |  | `start` (24) |  |
| `initial_nozzle_id` |  | `start` (24) |  |
| `max_print_z` | entero | `start` (24) |  |
| `min_vitrification_temperature` | entero | `start`, `layer_change`, `toolchange` (24) |  |
| `next_filament_id` | entero | `toolchange` (24) |  |
| `next_nozzle_id` | entero | `toolchange` (24) |  |
| `nozzle_diameter_at_nozzle_id` | lista de números | `start`, `toolchange`, `layer_change` (24) |  |
| `filament_tower_interface_print_temp` | lista de enteros | `toolchange` (20) |  |
| `filament_tower_interface_purge_volume` | lista de números | `toolchange` (20) |  |
| `is_all_bbl_filament` |  | `start`, `layer_change`, `toolchange` (20) |  |
| `is_prime_tower_interface` | sí/no | `toolchange` (20) |  |
| `machine_max_acceleration_extruding` | lista de números | `start` (19) | `machine_max_acceleration_extruding` |
| `machine_max_acceleration_retracting` | lista de números | `start` (19) | `machine_max_acceleration_retracting` |
| `layer_height` | número | `before_layer_change`, `start`, `file_start_gcode`, `toolchange_gcode` (18) | `layer_height` |
| `z_hop` | lista de números | `toolchange` (17) | `retract_lift` |
| `additional_cooling_fan_speed` | lista de enteros | `layer_change`, `toolchange`, `start` (16) |  |
| `bed_mesh_algo` |  | `start` (16) | `bicubic` |
| `cooling_filter_enabled` | sí/no | `start` (16) |  |
| `curr_physical_extruder_id` | entero | `timelapse` (16) |  |
| `farthest_point_timelapse_enabled` | sí/no | `timelapse` (16) |  |
| `first_non_support_filaments` | lista de enteros | `start` (16) |  |
| `has_timelapse_safe_pos` | sí/no | `timelapse` (16) |  |
| `has_tpu_in_first_layer` | sí/no | `start` (16) |  |
| `long_retraction_when_ec` |  | `toolchange`, `start`, `end` (16) |  |
| `most_used_physical_extruder_id` | entero | `timelapse` (16) |  |
| `retract_length_toolchange` | lista de números | `toolchange` (16) | `retract_length_toolchange` |
| `retraction_distance_when_ec` |  | `toolchange`, `start`, `end` (16) |  |
| `timelapse_inline_photo` | sí/no | `timelapse` (16) |  |
| `timelapse_pos_x` | entero | `timelapse` (16) |  |
| `timelapse_pos_y` | entero | `timelapse` (16) |  |
| `wipe_avoid_perimeter` | sí/no | `toolchange` (16) |  |
| `wipe_avoid_pos_x` | número | `toolchange` (16) |  |
| `day` | entero | `start` (13) | `day` |
| `hour` | entero | `start` (13) | `hour` |
| `minute` | entero | `start` (13) | `minute` |
| `month` | entero | `start` (13) | `month` |
| `second` | entero | `start` (13) | `second` |
| `toolchange_z` | número | `toolchange` (13) | `toolchange_z` |
| `year` | entero | `start` (13) | `year` |
| `current_hotend` | entero | `toolchange`, `end` (12) |  |
| `during_print_exhaust_fan_speed` | lista de enteros | `start`, `layer_change` (12) |  |
| `first_filaments` | lista de enteros | `start` (12) |  |
| `first_layer_center_no_wipe_tower` | lista de números | `timelapse`, `end` (12) |  |
| `hold_chamber_temp_for_flat_print` | sí/no | `start` (12) |  |
| `hotend_cooling_rate` | lista de números | `toolchange` (12) |  |
| `hotend_heating_rate` | lista de números | `toolchange` (12) |  |
| `initial_filament_id` |  | `start` (12) |  |
| `initial_no_support_hotend` |  | `start` (12) |  |
| `new_extruder_retracted_length` | número | `toolchange` (12) |  |
| `next_hotend` | entero | `toolchange` (12) |  |
| `nozzle_volume_types` | lista de textos | `toolchange` (12) |  |
| `print_bed_size` | lista de números | `start` (12) | `print_bed_size` |
| `scan_first_layer` | sí/no | `start` (12) |  |
| `travel_acceleration` | lista de números | `toolchange` (12) | `travel_acceleration` |
| `z_hop_types` | lista de opciones | `toolchange` (12) |  |
| `machine_max_acceleration_travel` | lista de números | `start` (11) | `machine_max_acceleration_travel` |
| `pressure_advance` | lista de números | `start` (11) |  |
| `notes` | texto | `start` (10) | `notes` |
| `enable_pressure_advance` | lista de sí/no | `start` (9) |  |
| `extruded_weight_total` | número | `before_layer_change`, `layer_change` (9) | `extruded_weight_total` |
| `additional_fan_full_speed_layer` | lista de enteros | `layer_change`, `toolchange` (8) |  |
| `close_additional_fan_first_x_layers` | lista de enteros | `layer_change`, `toolchange` (8) |  |
| `first_x_layer_fan_speed` | lista de números | `layer_change`, `toolchange` (8) |  |
| `in_head_wrap_detect_zone` |  | `timelapse` (8) |  |
| `max_additional_fan` |  | `layer_change`, `toolchange` (8) |  |
| `position` | lista de números | `toolchange`, `end` (8) | `position` |
| `filament_notes` | lista de textos | `start` (7) | `filament_notes` |
| `filament_settings_id` | lista de textos | `start`, `layer_change` (7) | `filament_settings_id` |
| `idle_temperature` | lista de enteros | `start` (7) | `idle_temperature` |
| `nozzle_temperature_initial_layer_0` |  | `start` (7) | `first_layer_temperature` |
| `nozzle_temperature_initial_layer_1` |  | `start` (7) |  |
| `extra_loading_move` | número | `start` (5) | `extra_loading_move` |
| `extrude_length_` | propia de la macro | `toolchange` (5) |  |
| `flush_length_` | propia de la macro | `toolchange` (5) |  |
| `loops_` | propia de la macro | `toolchange` (5) |  |
| `parking_pos_retraction` | número | `start` (5) | `parking_pos_retraction` |
| `then` |  | `toolchange`, `end` (5) |  |
| `adaptive_bed_mesh_margin` | número | `start` (4) |  |
| `clear_to_x0` | sí/no | `timelapse` (4) |  |
| `curr_y_acceleration_limit` | número | `layer_change` (4) |  |
| `filament_ids` | lista de textos | `start`, `toolchange` (4) |  |
| `filament_pre_cooling_temperature_nc` | lista de enteros | `toolchange` (4) |  |
| `filament_retract_length_nc` | lista de números | `toolchange` (4) |  |
| `first_non_support_hotend` | lista de enteros | `start` (4) |  |
| `has_wipe_tower` | sí/no | `toolchange` (4) | `has_wipe_tower` |
| `hot_plate_temp_initial_layer` | lista de enteros | `start` (4) |  |
| `max_speed_toolchange` | propia de la macro | `toolchange` (4) |  |
| `move_z` | propia de la macro | `end`, `toolchange` (4) |  |
| `nozzle_volume_type` | lista de opciones | `end` (4) |  |
| `parallel_printheads_count` | entero | `start`, `layer_change` (4) |  |
| `print_settings_id` | texto | `end` (4) | `print_settings_id` |
| `print_time_total_sec` | texto | `file_start_gcode` (4) |  |
| `printer_notes` | texto | `start` (4) | `printer_notes` |
| `speed_toolchange` | propia de la macro | `toolchange` (4) |  |
| `temperature_vitrification` | lista de enteros | `start`, `toolchange` (4) |  |
| `time_flush_` | propia de la macro | `toolchange` (4) |  |
| `used_filament_length` | texto | `file_start_gcode` (4) |  |
| `wait_for_extruder_temp` | propia de la macro | `toolchange` (4) |  |
| `wipe_tower_center_pos_valid` | sí/no | `start`, `toolchange` (4) |  |
| `wipe_tower_center_pos_x` | número | `start`, `toolchange` (4) |  |
| `wipe_tower_center_pos_y` | número | `start`, `toolchange` (4) |  |
| `e_retracted` | lista de números | `toolchange`, `start` (3) | `e_retracted` |
| `filament_diameter` | lista de números | `start`, `toolchange` (3) | `filament_diameter` |
| `machine_max_speed_x` | lista de números | `start`, `end` (3) |  |
| `machine_max_speed_z` | lista de números | `start`, `end` (3) |  |
| `required_nozzle_HRC` | lista de enteros | `start` (3) |  |
| `retraction_speed` | lista de números | `toolchange`, `start` (3) |  |
| `total_toolchanges` | entero | `toolchange`, `start` (3) | `total_toolchanges` |
| `chamber_minimal_temperature` | lista de enteros | `start` (2) | `chamber_minimal_temperature` |
| `deretract_length` | propia de la macro | `toolchange`, `start` (2) |  |
| `deretraction_speed` | lista de números | `start`, `toolchange` (2) |  |
| `eject_temp` | propia de la macro | `start`, `toolchange` (2) |  |
| `enable_prime_tower` | sí/no | `toolchange` (2) |  |
| `extruded_volume` | lista de números | `start` (2) | `extruded_volume` |
| `fan_max_speed` | lista de números | `start`, `toolchange` (2) |  |
| `filament_area` | propia de la macro | `start`, `toolchange` (2) |  |
| `filament_flush_volumetric_speed` | lista de números | `start`, `toolchange` (2) |  |
| `filament_minimal_purge_on_wipe_tower` | lista de números | `toolchange` (2) | `filament_minimal_purge_on_wipe_tower` |
| `filament_retract_length_toolchange` |  | `toolchange` (2) |  |
| `first_layer_height` | número | `start` (2) | `first_layer_height` |
| `inner_wall_speed` | lista de números | `start` (2) |  |
| `low_temp_types` | propia de la macro | `layer_change`, `start` (2) |  |
| `machine_max_acceleration_e` | lista de números | `start` (2) | `machine_max_acceleration_e` |
| `machine_max_acceleration_x` | lista de números | `start` (2) | `machine_max_acceleration_x` |
| `machine_max_acceleration_y` | lista de números | `start` (2) | `machine_max_acceleration_y` |
| `machine_max_acceleration_z` | lista de números | `start` (2) | `machine_max_acceleration_z` |
| `machine_max_jerk_e` | lista de números | `start` (2) | `machine_max_jerk_e` |
| `machine_max_jerk_x` | lista de números | `start` (2) | `machine_max_jerk_x` |
| `machine_max_jerk_y` | lista de números | `start` (2) | `machine_max_jerk_y` |
| `machine_max_jerk_z` | lista de números | `start` (2) | `machine_max_jerk_z` |
| `machine_max_speed_e` | lista de números | `start` (2) |  |
| `machine_max_speed_y` | lista de números | `start` (2) |  |
| `outer_wall_line_width` | número o porcentaje | `start` (2) |  |
| `outer_wall_speed` | lista de números | `start` (2) |  |
| `perimeter_flow_rate` | propia de la macro | `start` (2) |  |
| `print_preset` | texto | `start` (2) | `print_preset` |
| `probe_temp` | propia de la macro | `start` (2) |  |
| `purge_mm` | propia de la macro | `start`, `toolchange` (2) |  |
| `purge_speed_fast` | propia de la macro | `start`, `toolchange` (2) |  |
| `purge_speed_slow` | propia de la macro | `start`, `toolchange` (2) |  |
| `retract_restart_extra_toolchange` | lista de números | `start`, `toolchange` (2) | `retract_restart_extra_toolchange` |
| `retract_toolchange` | propia de la macro | `toolchange`, `start` (2) |  |
| `speed_tc` | propia de la macro | `toolchange`, `start` (2) |  |
| `target_temp` | propia de la macro | `toolchange`, `start` (2) |  |
| `tc_deretract_speed` | propia de la macro | `toolchange`, `start` (2) |  |
| `tc_retract_speed` | propia de la macro | `toolchange` (2) |  |
| `tool_init` | propia de la macro | `toolchange`, `start` (2) |  |
| `travel_slope` | lista de números | `toolchange` (2) | `travel_slope` |
| `used_tools` | propia de la macro | `start`, `layer_change` (2) |  |
| `EXTRUDE_SPEED_` | propia de la macro | `toolchange` (1) |  |
| `UNWIND_SPEED_` | propia de la macro | `toolchange` (1) |  |
| `curr_t` | propia de la macro | `toolchange` (1) |  |
| `filament_multitool_ramming` | lista de sí/no | `start` (1) | `filament_multitool_ramming` |
| `first_flush_volume` | número | `toolchange` (1) |  |
| `index_` | propia de la macro | `toolchange` (1) |  |
| `initial_layer_speed` | lista de números | `start` (1) |  |
| `input_filename_base` | texto | `start` (1) | `input_filename_base` |
| `line_width` | número o porcentaje | `start` (1) |  |
| `minimal_extrude_` | propia de la macro | `toolchange` (1) |  |
| `new_retract_length` | número | `toolchange` (1) |  |
| `next_t` | propia de la macro | `toolchange` (1) |  |
| `nozzle_temperature_range_low` | lista de enteros | `start` (1) |  |
| `retract_length` | lista de números | `start` (1) | `retract_length` |
| `step_from_` | propia de la macro | `toolchange` (1) |  |
| `step_into_` | propia de la macro | `toolchange` (1) |  |
| `tab_step_time_` | propia de la macro | `toolchange` (1) |  |
## `crealityprint`

38 variables.

| Variable | Tipo | Dónde | Equivalente |
|---|---|---|---|
| `bed_temperature_initial_layer_single` | entero | `start` (16) | `first_layer_bed_temperature` |
| `layer_z` | número | `before_layer_change`, `layer_change` (16) | `layer_z` |
| `nozzle_temperature_initial_layer` | lista de enteros | `start` (16) | `first_layer_temperature` |
| `initial_no_support_extruder` |  | `start` (14) | `initial_extruder` |
| `z_after_toolchange` | número | `toolchange` (14) |  |
| `filament_max_volumetric_speed` | lista de números | `start` (10) | `filament_max_volumetric_speed` |
| `initial_extruder` | entero | `start` (10) | `initial_extruder` |
| `max_layer_z` | número | `end`, `toolchange` (7) | `max_layer_z` |
| `print_sequence` | opción | `end`, `toolchange` (5) |  |
| `max_print_height` | entero | `end` (2) | `max_print_height` |
| `prime_tower_width` | número | `toolchange` (2) |  |
| `print_bed_max` | lista de números | `end` (2) | `print_bed_max` |
| `printable_height` | número | `end` (2) | `max_print_height` |
| `travel_speed` | lista de números | `end` (2) | `travel_speed` |
| `wipe_tower_outer_wall_x` | número | `toolchange` (2) |  |
| `wipe_tower_outer_wall_y` | número | `toolchange` (2) |  |
| `wipe_tower_random_num` | número | `toolchange` (2) |  |
| `z_offset` | número | `end` (2) | `z_offset` |
| `flush_length_1` |  | `toolchange` (1) |  |
| `flush_length_2` |  | `toolchange` (1) |  |
| `flush_length_3` |  | `toolchange` (1) |  |
| `flush_length_4` |  | `toolchange` (1) |  |
| `flush_length_5` |  | `toolchange` (1) |  |
| `long_retractions_when_cut` | lista de sí/no | `toolchange` (1) |  |
| `multicolor_method` | sí/no | `start`, `toolchange` (1) |  |
| `new_filament_e_feedrate` | entero | `toolchange` (1) |  |
| `new_filament_temp` | entero | `toolchange` (1) |  |
| `new_retract_length_toolchange` | número | `toolchange` (1) |  |
| `next_extruder` | entero | `toolchange` (1) | `next_extruder` |
| `nozzle_temperature_range_high` | lista de enteros | `toolchange`, `start` (1) |  |
| `old_filament_e_feedrate` | entero | `toolchange` (1) |  |
| `old_retract_length_toolchange` | número | `toolchange` (1) |  |
| `outer_wall_volumetric_speed` | número | `start` (1) |  |
| `position` | lista de números | `toolchange` (1) | `position` |
| `previous_extruder` | entero | `toolchange` (1) | `previous_extruder` |
| `retract_length_toolchange` | lista de números | `start` (1) | `retract_length_toolchange` |
| `retraction_distances_when_cut` | lista de números | `toolchange` (1) |  |
| `toolchange_count` | entero | `toolchange` (1) |  |
## `elegooslicer`

6 variables.

| Variable | Tipo | Dónde | Equivalente |
|---|---|---|---|
| `first_layer_print_max` | lista de números | `start` (1) | `first_layer_print_max` |
| `first_layer_print_min` | lista de números | `start` (1) | `first_layer_print_min` |
| `first_layer_temperature` | lista de enteros | `start` (1) | `first_layer_temperature` |
| `initial_extruder` | entero | `start` (1) | `initial_extruder` |
| `is_extruder_used` | lista de sí/no | `start` (1) | `is_extruder_used` |
| `layer_z` | número | `before_layer_change`, `layer_change` (1) | `layer_z` |
## `cura`

33 variables.

| Variable | Tipo | Dónde | Equivalente |
|---|---|---|---|
| `material_print_temperature_layer_0` | número | `start` (65) | `first_layer_temperature` |
| `material_bed_temperature_layer_0` | número | `start` (51) | `first_layer_bed_temperature` |
| `machine_depth` | número | `end` (31) |  |
| `material_print_temperature` | número | `start` (27) | `temperature` |
| `material_bed_temperature` | número | `start` (19) | `bed_temperature` |
| `speed_travel` | número | `start`, `end` (19) | `travel_speed` |
| `material_type` | texto | `start` (14) | `filament_type` |
| `infill_sparse_density` | número | `start` (12) |  |
| `date` |  | `start` (7) |  |
| `machine_name` | texto | `start` (7) |  |
| `time` |  | `start` (7) |  |
| `build_volume_temperature` | número | `start` (6) |  |
| `day` |  | `start` (5) | `day` |
| `filament_weight` |  | `start` (5) |  |
| `initial_extruder_nr` |  | `start` (5) | `initial_extruder` |
| `layer_height` | número | `start` (5) | `layer_height` |
| `wall_thickness` | número | `start` (5) |  |
| `layer_height_0` | número | `start` (4) | `first_layer_height` |
| `machine_height` | número | `end` (4) |  |
| `machine_nozzle_size` | número | `start` (4) | `nozzle_diameter` |
| `material_name` | texto | `start` (4) | `filament_settings_id` |
| `material_standby_temperature` | número | `start`, `end` (4) |  |
| `print_bed_temperature` |  | `start` (4) | `bed_temperature` |
| `print_time` |  | `start` (2) | `print_time` |
| `filament_amount` |  | `start` (1) |  |
| `filament_cost` |  | `start` (1) | `filament_cost` |
| `jobname` |  | `end` (1) |  |
| `print_temperature` |  | `start` (1) |  |
| `purge_distance` |  | `extruder_start` (1) |  |
| `retraction_hop_height_after_extruder_switch` |  | `extruder_start` (1) |  |
| `switch_extruder_retraction_amount` | número | `extruder_start` (1) |  |
| `top_bottom_thickness` | número | `start` (1) |  |
| `z` |  | `start` (1) |  |
