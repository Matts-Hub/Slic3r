#include "PrintConfig.hpp"
#include "I18N.hpp"

#include <set>
#include <boost/algorithm/string/replace.hpp>
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/format.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/thread.hpp>

#include <float.h>

namespace Slic3r {

//! macro used to mark string used at localization, 
//! return same string
#define L(s) (s)
#define _(s) Slic3r::I18N::translate(s)

static void assign_printer_technology_to_unknown(t_optiondef_map &options, PrinterTechnology printer_technology)
{
    for (std::pair<const t_config_option_key, ConfigOptionDef> &kvp : options)
        if (kvp.second.printer_technology == ptUnknown)
            kvp.second.printer_technology = printer_technology;
}

PrintConfigDef::PrintConfigDef()
{
    this->init_common_params();
	//assign params that are not already allocated to FFF+SLA (default from slic3rPE)
    assign_printer_technology_to_unknown(this->options, ptFFF | ptSLA);
    this->init_fff_params();
    this->init_extruder_retract_keys();
    assign_printer_technology_to_unknown(this->options, ptFFF);
    this->init_sla_params();
    assign_printer_technology_to_unknown(this->options, ptSLA);
}

void PrintConfigDef::init_common_params()
{
    ConfigOptionDef* def;

    def = this->add("printer_technology", coEnum);
    def->label = L("Printer technology");
    def->tooltip = L("Printer technology");
    def->enum_keys_map = &ConfigOptionEnum<PrinterTechnology>::get_enum_values();
    def->enum_values.push_back("FFF");
    def->enum_values.push_back("SLA");
    def->set_default_value(new ConfigOptionEnum<PrinterTechnology>(ptFFF));

    def = this->add("bed_shape", coPoints);
    def->label = L("Bed shape");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionPoints{ Vec2d(0, 0), Vec2d(200, 0), Vec2d(200, 200), Vec2d(0, 200) });
    
    def = this->add("bed_custom_texture", coString);
    def->label = L("Bed custom texture");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionString(""));

    def = this->add("bed_custom_model", coString);
    def->label = L("Bed custom model");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionString(""));

    def = this->add("layer_height", coFloat);
    def->label = L("Base Layer height");
    def->category = L("Layers and Perimeters");
    def->tooltip = L("This setting controls the height (and thus the total number) of the slices/layers. "
                   "Thinner layers give better accuracy but take more time to print.");
    def->sidetext = L("mm");
    def->min = 0;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloat(0.2));

    def = this->add("max_print_height", coFloat);
    def->label = L("Max print height");
    def->tooltip = L("Set this to the maximum height that can be reached by your extruder while printing.");
    def->sidetext = L("mm");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloat(200.0));

    def = this->add("slice_closing_radius", coFloat);
    def->label = L("Slice gap closing radius");
    def->category = L("Advanced");
    def->tooltip = L("Cracks smaller than 2x gap closing radius are being filled during the triangle mesh slicing. "
                     "The gap closing operation may reduce the final print resolution, therefore it is advisable to keep the value reasonably low.");
    def->sidetext = L("mm");
    def->min = 0;
	def->mode = comAdvanced;
	def->set_default_value(new ConfigOptionFloat(0.049));

    def = this->add("print_host", coString);
    def->label = L("Hostname, IP or URL");
    def->tooltip = L("Slic3r can upload G-code files to a printer host. This field should contain "
                   "the hostname, IP address or URL of the printer host instance.");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionString(""));

    def = this->add("printhost_apikey", coString);
    def->label = L("API Key / Password");
    def->tooltip = L("Slic3r can upload G-code files to a printer host. This field should contain "
                   "the API Key or the password required for authentication.");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionString(""));
    
    def = this->add("printhost_cafile", coString);
    def->label = L("HTTPS CA File");
    def->tooltip = L("Custom CA certificate file can be specified for HTTPS OctoPrint connections, in crt/pem format. "
                   "If left blank, the default OS CA certificate repository is used.");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionString(""));
}

void PrintConfigDef::init_fff_params()
{
    ConfigOptionDef* def;

    // Maximum extruder temperature, bumped to 1500 to support printing of glass.
    const int max_temp = 1500;

    def = this->add("avoid_crossing_perimeters", coBool);
    def->label = L("Avoid crossing perimeters");
    def->tooltip = L("Optimize travel moves in order to minimize the crossing of perimeters. "
                   "This is mostly useful with Bowden extruders which suffer from oozing. "
                   "This feature slows down both the print and the G-code generation.");
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionBool(false));

    def = this->add("bed_temperature", coInts);
    def->label = L("Other layers");
    def->tooltip = L("Bed temperature for layers after the first one. "
                   "Set this to zero to disable bed temperature control commands in the output.");
    def->full_label = L("Bed temperature");
    def->sidetext = L("°C");
    def->min = 0;
    def->max = 300;
    def->set_default_value(new ConfigOptionInts { 0 });

    def = this->add("before_layer_gcode", coString);
    def->label = L("Before layer change G-code");
    def->tooltip = L("This custom code is inserted at every layer change, right before the Z move. "
                   "Note that you can use placeholder variables for all Slic3r settings as well "
                   "as [layer_num] and [layer_z].");
    def->multiline = true;
    def->full_width = true;
    def->height = 5;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionString(""));

    def = this->add("between_objects_gcode", coString);
    def->label = L("Between objects G-code");
    def->tooltip = L("This code is inserted between objects when using sequential printing. By default extruder and bed temperature are reset using non-wait command; however if M104, M109, M140 or M190 are detected in this custom code, Slic3r will not add temperature commands. Note that you can use placeholder variables for all Slic3r settings, so you can put a \"M109 S[first_layer_temperature]\" command wherever you want.");
    def->multiline = true;
    def->full_width = true;
    def->height = 12;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionString(""));

    def = this->add("bottom_solid_layers", coInt);
    //TRN To be shown in Print Settings "Bottom solid layers"
    def->label = L("Bottom");
    def->category = L("Layers and Perimeters");
    def->tooltip = L("Number of solid layers to generate on bottom surfaces.");
    def->full_label = L("Bottom solid layers");    def->min = 0;
    def->set_default_value(new ConfigOptionInt(3));

    def = this->add("bridge_acceleration", coFloat);
    def->label = L("Bridge");
    def->full_label = L("Bridge acceleration");
    def->tooltip = L("This is the acceleration your printer will use for bridges. "
                   "Set zero to disable acceleration control for bridges.");
    def->sidetext = L("mm/s²");
    def->min = 0;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloat(0));

    def = this->add("bridge_angle", coFloat);
    def->label = L("Bridging");
    def->full_label = L("Bridging angle");
    def->category = L("Infill");
    def->tooltip = L("Bridging angle override. If left to zero, the bridging angle will be calculated "
                   "automatically. Otherwise the provided angle will be used for all bridges. "
                   "Use 180° for zero angle.");
    def->sidetext = L("°");
    def->min = 0;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloat(0.));

    def = this->add("bridge_fan_speed", coInts);
    def->label = L("Bridges fan speed");
    def->tooltip = L("This fan speed is enforced during all bridges and overhangs.");
    def->sidetext = L("%");
    def->min = 0;
    def->max = 100;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionInts { 100 });

    def = this->add("top_fan_speed", coInts);
    def->label = L("Top fan speed");
    def->tooltip = L("This fan speed is enforced during all top fills.");
    def->sidetext = L("%");
    def->min = 0;
    def->max = 100;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionInts{ 100 });

    def = this->add("bridge_flow_ratio", coFloat);
    def->label = L("Bridge");
    def->full_label = L("Bridge flow ratio");
    def->category = L("Advanced");
    def->tooltip = L("This factor affects the amount of plastic for bridging. "
                   "You can decrease it slightly to pull the extrudates and prevent sagging, "
                   "although default settings are usually good and you should experiment "
                   "with cooling (use a fan) before tweaking this.");
    def->min = 0;
    def->max = 2;
    def->mode = comAdvanced;
	def->set_default_value(new ConfigOptionFloat(1));

    def = this->add("over_bridge_flow_ratio", coFloat);
    def->label = L("Above the bridges");
    def->full_label = L("Above bridge flow ratio");
    def->category = L("Advanced");
    def->tooltip = L("Flow ratio to compensate for the gaps in a bridged top surface. Used for ironing infill"
        "pattern to prevent regions where the low-flow pass does not provide a smooth surface due to a lack of plastic."
        " You can increase it slightly to pull the top layer at the correct height. Recommended maximum: 1.2.");
    def->min = 0;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloat(1));

    def = this->add("top_infill_flow_ratio", coFloat);
    def->label = L("Top solid infill");
    def->full_label = L("Top solid infill flow ratio");
    def->category = L("Advanced");
    def->tooltip = L("Flow ratio for the top solid infill layer. You can decrease this to allow space for ironing flow if it is being used.");
    def->min = 0;
    def->max = 2;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloat(1));

    def = this->add("ironing_flow_ratio", coFloat);
    def->label = L("Ironing");
    def->full_label = L("Ironing flow ratio");
    def->category = L("Advanced");
    def->tooltip = L("Flow ratio for the ironing layer if enabled. Recommended value is zero, but you can increase this if you are experiencing gaps in your ironing layer.");
    def->min = 0;
    def->max = 1;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloat(0));

    def = this->add("bridge_speed", coFloat);
    def->label = L("Bridges");
    def->full_label = L("Bridge speed");
    def->category = L("Speed");
    def->tooltip = L("Speed for printing bridges.");
    def->sidetext = L("mm/s");
    def->aliases = { "bridge_feed_rate" };
    def->min = 0;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloat(60));

    def = this->add("brim_width", coFloat);
    def->label = L("Brim width");
    def->tooltip = L("Horizontal width of the brim that will be printed around each object on the first layer.");
    def->sidetext = L("mm");
    def->min = 0;
    def->mode = comSimple;
    def->set_default_value(new ConfigOptionFloat(0));

    def = this->add("brim_ears", coBool);
    def->label = L(" ");
    def->full_label = L("Brim ears");
    def->tooltip = L("Only draw brim over the sharp edges of the model.");
    def->set_default_value(new ConfigOptionBool(false));

    def = this->add("brim_ears_max_angle", coFloat);
    def->label = L("max angle");
    def->tooltip = L("Maximum angle to let a brim ear appear. \nIf set to 0, no brim will be created. \nIf set to ~178, brim will be created on everything but strait sections.");
    def->sidetext = L("°");
    def->min = 0;
    def->max = 180;
    def->set_default_value(new ConfigOptionFloat(125));

    def = this->add("chamber_temperature", coInts);
    def->label = L("Chamber");
    def->full_label = L("Chamber temperature");
    def->tooltip = L("Chamber temperature0. Note that this setting doesn't do anything, but you can access it in Start G-code, Tool change G-code and the other ones, like for other temperature settings.");
    def->sidetext = L("°C");
    def->min = 0;
    def->max = 300;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionInts{ 0 });

    def = this->add("clip_multipart_objects", coBool);
    def->label = L("Clip multi-part objects");
    def->tooltip = L("When printing multi-material objects, this settings will make Slic3r "
                   "to clip the overlapping object parts one by the other "
                   "(2nd part will be clipped by the 1st, 3rd part will be clipped by the 1st and 2nd etc).");
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionBool(false));

    def = this->add("colorprint_heights", coFloats);
    def->label = L("Colorprint height");
    def->tooltip = L("Heights at which a filament change is to occur. ");
    def->set_default_value(new ConfigOptionFloats { });

    def = this->add("compatible_printers", coStrings);
    def->label = L("Compatible printers");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionStrings());
    def->cli = ConfigOptionDef::nocli;

    def = this->add("compatible_printers_condition", coString);
    def->label = L("Compatible printers condition");
    def->tooltip = L("A boolean expression using the configuration values of an active printer profile. "
                   "If this expression evaluates to true, this profile is considered compatible "
                   "with the active printer profile.");
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionString());
    def->cli = ConfigOptionDef::nocli;

    def = this->add("compatible_prints", coStrings);
    def->label = L("Compatible print profiles");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionStrings());
    def->cli = ConfigOptionDef::nocli;

    def = this->add("compatible_prints_condition", coString);
    def->label = L("Compatible print profiles condition");
    def->tooltip = L("A boolean expression using the configuration values of an active print profile. "
                   "If this expression evaluates to true, this profile is considered compatible "
                   "with the active print profile.");
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionString());
    def->cli = ConfigOptionDef::nocli;

    // The following value is to be stored into the project file (AMF, 3MF, Config ...)
    // and it contains a sum of "compatible_printers_condition" values over the print and filament profiles.
    def = this->add("compatible_printers_condition_cummulative", coStrings);
    def->set_default_value(new ConfigOptionStrings());
    def->cli = ConfigOptionDef::nocli;
    def = this->add("compatible_prints_condition_cummulative", coStrings);
    def->set_default_value(new ConfigOptionStrings());
    def->cli = ConfigOptionDef::nocli;

    def = this->add("complete_objects", coBool);
    def->label = L("Complete individual objects");
    def->tooltip = L("When printing multiple objects or copies, this feature will complete "
                   "each object before moving onto next one (and starting it from its bottom layer). "
                   "This feature is useful to avoid the risk of ruined prints. "
                   "Slic3r should warn and prevent you from extruder collisions, but beware.");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionBool(false));

    def = this->add("cooling", coBools);
    def->label = L("Enable auto cooling");
    def->tooltip = L("This flag enables the automatic cooling logic that adjusts print speed "
                   "and fan speed according to layer printing time.");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionBools { true });

    def = this->add("cooling_tube_retraction", coFloat);
    def->label = L("Cooling tube position");
    def->tooltip = L("Distance of the center-point of the cooling tube from the extruder tip.");
    def->sidetext = L("mm");
    def->min = 0;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloat(91.5f));

    def = this->add("cooling_tube_length", coFloat);
    def->label = L("Cooling tube length");
    def->tooltip = L("Length of the cooling tube to limit space for cooling moves inside it.");
    def->sidetext = L("mm");
    def->min = 0;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloat(5.f));

    def = this->add("default_acceleration", coFloat);
    def->label = L("Default");
    def->full_label = L("Default acceleration");
    def->tooltip = L("This is the acceleration your printer will be reset to after "
                   "the role-specific acceleration values are used (perimeter/infill). "
                   "Set zero to prevent resetting acceleration at all.");
    def->sidetext = L("mm/s²");
    def->min = 0;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloat(0));

    def = this->add("default_filament_profile", coStrings);
    def->label = L("Default filament profile");
    def->tooltip = L("Default filament profile associated with the current printer profile. "
                   "On selection of the current printer profile, this filament profile will be activated.");
    def->set_default_value(new ConfigOptionStrings());
    def->cli = ConfigOptionDef::nocli;

    def = this->add("default_print_profile", coString);
    def->label = L("Default print profile");
    def->tooltip = L("Default print profile associated with the current printer profile. "
                   "On selection of the current printer profile, this print profile will be activated.");
    def->set_default_value(new ConfigOptionString());
    def->cli = ConfigOptionDef::nocli;

    def = this->add("disable_fan_first_layers", coInts);
    def->label = L("Disable fan for the first");
    def->tooltip = L("You can set this to a positive value to disable fan at all "
                   "during the first layers, so that it does not make adhesion worse.");
    def->sidetext = L("layers");
    def->min = 0;
    def->max = 1000;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionInts { 3 });

    def = this->add("dont_support_bridges", coBool);
    def->label = L("Don't support bridges");
    def->category = L("Support material");
    def->tooltip = L("Experimental option for preventing support material from being generated "
                   "under bridged areas.");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionBool(true));

    def = this->add("duplicate_distance", coFloat);
    def->label = L("Distance between copies");
    def->tooltip = L("Distance used for the auto-arrange feature of the plater.");
    def->sidetext = L("mm");
    def->aliases = { "multiply_distance" };
    def->min = 0;
    def->set_default_value(new ConfigOptionFloat(6));

    def = this->add("elefant_foot_compensation", coFloat);
    def->label = L("First layer");
    def->full_label = L("First layer compensation");
    def->category = L("Slicing");
    def->tooltip = L("The first layer will be grown / shrunk in the XY plane by the configured value "
                   "to compensate for the 1st layer squish aka an Elephant Foot effect. (should be negative = inwards)");
    def->sidetext = L("mm");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloat(0));

    def = this->add("end_gcode", coString);
    def->label = L("End G-code");
    def->tooltip = L("This end procedure is inserted at the end of the output file. "
                   "Note that you can use placeholder variables for all Slic3r settings.");
    def->multiline = true;
    def->full_width = true;
    def->height = 12;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionString("M104 S0 ; turn off temperature\nG28 X0  ; home X axis\nM84     ; disable motors\n"));

    def = this->add("end_filament_gcode", coStrings);
    def->label = L("End G-code");
    def->full_label = L("Filament end G-code");
    def->tooltip = L("This end procedure is inserted at the end of the output file, before the printer end gcode (and "
                   "before any toolchange from this filament in case of multimaterial printers). "
                   "Note that you can use placeholder variables for all Slic3r settings. "
                   "If you have multiple extruders, the gcode is processed in extruder order.");
    def->multiline = true;
    def->full_width = true;
    def->height = 120;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionStrings { "; Filament-specific end gcode \n;END gcode for filament\n" });

    def = this->add("ensure_vertical_shell_thickness", coBool);
    def->label = L("Ensure vertical shell thickness");
    def->category = L("Layers and Perimeters");
    def->tooltip = L("Add solid infill near sloping surfaces to guarantee the vertical shell thickness "
                   "(top+bottom solid layers).");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionBool(false));

    def = this->add("top_fill_pattern", coEnum);
    def->label = L("Top");
    def->full_label = L("Top Pattern");
    def->category = L("Infill");
    def->tooltip = L("Fill pattern for top infill. This only affects the top visible layer, and not its adjacent solid shells.");
    def->cli = "top-fill-pattern|external-fill-pattern=s";
    def->enum_keys_map = &ConfigOptionEnum<InfillPattern>::get_enum_values();
    def->enum_values.push_back("rectilinear");
    def->enum_values.push_back("rectilineargapfill");
    def->enum_values.push_back("concentric");
    def->enum_values.push_back("concentricgapfill");
    def->enum_values.push_back("hilbertcurve");
    def->enum_values.push_back("archimedeanchords");
    def->enum_values.push_back("octagramspiral");
    def->enum_values.push_back("sawtooth");
    def->enum_values.push_back("smooth");
    def->enum_values.push_back("smoothtriple");
    def->enum_values.push_back("smoothhilbert");
    def->enum_labels.push_back(L("Rectilinear"));
    def->enum_labels.push_back(L("Rectilinear (filled)"));
    def->enum_labels.push_back(L("Concentric"));
    def->enum_labels.push_back(L("Concentric (filled)"));
    def->enum_labels.push_back(L("Hilbert Curve"));
    def->enum_labels.push_back(L("Archimedean Chords"));
    def->enum_labels.push_back(L("Octagram Spiral"));
    def->enum_labels.push_back(L("Sawtooth"));
    def->enum_labels.push_back(L("Ironing"));
    def->set_default_value(new ConfigOptionEnum<InfillPattern>(ipRectilinear));

    def = this->add("bottom_fill_pattern", coEnum);
    def->label = L("Bottom");
    def->full_label = L("Bottom fill pattern");
    def->category = L("Infill");
    def->tooltip = L("Fill pattern for bottom infill. This only affects the bottom visible layer, and not its adjacent solid shells.");
    def->cli = "bottom-fill-pattern|external-fill-pattern=s";
    def->enum_keys_map = &ConfigOptionEnum<InfillPattern>::get_enum_values();
    def->enum_values.push_back("rectilinear");
    def->enum_values.push_back("rectilineargapfill");
    def->enum_values.push_back("concentric");
    def->enum_values.push_back("concentricgapfill");
    def->enum_values.push_back("hilbertcurve");
    def->enum_values.push_back("archimedeanchords");
    def->enum_values.push_back("octagramspiral");
    def->enum_values.push_back("smooth");
    def->enum_labels.push_back(L("Rectilinear (filled)"));
    def->enum_labels.push_back(L("Rectilinear"));
    def->enum_labels.push_back(L("Concentric"));
    def->enum_labels.push_back(L("Concentric (filled)"));
    def->enum_labels.push_back(L("Hilbert Curve"));
    def->enum_labels.push_back(L("Archimedean Chords"));
    def->enum_labels.push_back(L("Octagram Spiral"));
    def->enum_labels.push_back(L("Ironing"));
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionEnum<InfillPattern>(ipRectilinear));

    def = this->add("solid_fill_pattern", coEnum);
    def->label = L("Solid pattern");
    def->category = L("Infill");
    def->tooltip = L("Fill pattern for solid (internal) infill. This only affects the solid not-visible layers. You should use rectilinear is most cases. You can try ironing for transluscnet material."
        " Rectilinear (filled) replace zig-zag patterns by a single big line & is more efficient for filling little spaces.");
    def->enum_keys_map = &ConfigOptionEnum<InfillPattern>::get_enum_values();
    def->enum_values.push_back("smooth");
    def->enum_values.push_back("rectilinear");
    def->enum_values.push_back("rectilineargapfill");
    def->enum_values.push_back("concentric");
    def->enum_values.push_back("concentricgapfill");
    def->enum_values.push_back("hilbertcurve");
    def->enum_values.push_back("archimedeanchords");
    def->enum_values.push_back("octagramspiral");
    def->enum_labels.push_back(L("Ironing"));
    def->enum_labels.push_back(L("Rectilinear"));
    def->enum_labels.push_back(L("Rectilinear (filled)"));
    def->enum_labels.push_back(L("Concentric"));
    def->enum_labels.push_back(L("Concentric (filled)"));
    def->enum_labels.push_back(L("Hilbert Curve"));
    def->enum_labels.push_back(L("Archimedean Chords"));
    def->enum_labels.push_back(L("Octagram Spiral"));
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionEnum<InfillPattern>(ipRectilinearWGapFill));

    def = this->add("enforce_full_fill_volume", coBool);
    def->label = L("Enforce 100% fill volume");
    def->category = L("Infill");
    def->tooltip = L("Experimental option which modifies (in solid infill) fill flow to have the exact amount of plastic inside the volume to fill "
        "(it generally changes the flow from -7% to +4%, depending on the size of the surface to fill and the overlap parameters, "
        "but it can go as high as +50% for infill in very small areas where rectilinear doesn't have good coverage). It has the advantage "
        "to remove the over-extrusion seen in thin infill areas, from the overlap ratio");
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionBool(true));

    def = this->add("external_infill_margin", coFloatOrPercent);
    def->label = L("Default");
    def->full_label = L("Default infill margin");
    def->category = L("Infill");
    def->tooltip = L("This parameter grows the top/bottom/solid layers by the specified MM to anchor them into the part. Put 0 to deactivate it. Can be a % of the width of the perimeters.");
    def->sidetext = L("mm");
    def->min = 0;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloatOrPercent(150, true));

    def = this->add("bridged_infill_margin", coFloatOrPercent);
    def->label = L("Bridged");
    def->full_label = L("Bridge margin");
    def->category = L("Infill");
    def->tooltip = L("This parameter grows the bridged solid infill layers by the specified MM to anchor them into the part. Put 0 to deactivate it. Can be a % of the width of the external perimeter.");
    def->sidetext = L("mm");
    def->min = 0;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloatOrPercent(200, true));

    def = this->add("external_perimeter_extrusion_width", coFloatOrPercent);
    def->label = L("External perimeters");
    def->full_label = L("External perimeters width");
    def->category = L("Extrusion Width");
    def->tooltip = L("Set this to a non-zero value to set a manual extrusion width for external perimeters. "
                   "If left zero, default extrusion width will be used if set, otherwise 1.125 x nozzle diameter will be used. "
                   "If expressed as percentage (for example 112.5%), it will be computed over nozzle diameter.");
    def->sidetext = L("mm or %");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloatOrPercent(0, false));

    def = this->add("external_perimeter_speed", coFloatOrPercent);
    def->label = L("External");
    def->full_label = L("External perimeters speed");
    def->category = L("Speed");
    def->tooltip = L("This separate setting will affect the speed of external perimeters (the visible ones). "
                   "If expressed as percentage (for example: 80%) it will be calculated "
                   "on the perimeters speed setting above. Set to zero for auto.");
    def->sidetext = L("mm/s or %");
    def->ratio_over = "perimeter_speed";
    def->min = 0;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloatOrPercent(50, true));

    def = this->add("external_perimeters_first", coBool);
    def->label = L("External perimeters first");
    def->category = L("Layers and Perimeters");
    def->tooltip = L("Print contour perimeters from the outermost one to the innermost one "
        "instead of the default inverse order.");
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionBool(false));

    def = this->add("perimeter_loop", coBool);
    def->label = L(" ");
    def->full_label = L("Perimeters loop");
    def->category = L("Layers and Perimeters");
    def->tooltip = L("Join the perimeters to create only one continuous extrusion without any z-hop."
        " Long inside travel (from external to holes) are not extruded to give some space to the infill.");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionBool(false));
    
    def = this->add("perimeter_loop_seam", coEnum);
    def->label = L("Seam position");
    def->full_label = L("Perimeter loop");
    def->category = L("Layers and Perimeters");
    def->tooltip = L("Position of perimeters starting points.");
    def->enum_keys_map = &ConfigOptionEnum<SeamPosition>::get_enum_values();
    def->enum_values.push_back("nearest");
    def->enum_values.push_back("rear");
    def->enum_labels.push_back(L("Nearest"));
    def->enum_labels.push_back(L("Rear"));
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionEnum<SeamPosition>(spRear)); 
    
    def = this->add("extra_perimeters", coBool);
    def->label = L("Extra perimeters if needed");
    def->category = L("Layers and Perimeters");
    def->tooltip = L("Add more perimeters when needed for avoiding gaps in sloping walls. "
                   "Slic3r keeps adding perimeters, until more than 70% of the loop immediately above "
                   "is supported, and keep adding periemter until all overhangs are filled."
                   "\n!! this is a very slow algorithm !!");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionBool(true));

    def = this->add("only_one_perimeter_top", coBool);
    def->label = "Only one perimeter on Top surfaces";
    def->category = "Layers and Perimeters";
    def->tooltip = "Use only one perimeter on flat top surface, to let more space to the top infill pattern.";
    def->set_default_value(new ConfigOptionBool(true));


    def = this->add("extruder", coInt);
    def->gui_type = "i_enum_open";
    def->label = L("Extruder");
    def->category = L("Extruders");
    def->tooltip = L("The extruder to use (unless more specific extruder settings are specified). "
                   "This value overrides perimeter and infill extruders, but not the support extruders.");
    def->min = 0;  // 0 = inherit defaults
    def->enum_labels.push_back("default");  // override label for item 0
    def->enum_labels.push_back("1");
    def->enum_labels.push_back("2");
    def->enum_labels.push_back("3");
    def->enum_labels.push_back("4");
    def->enum_labels.push_back("5");
    def->enum_labels.push_back("6");
    def->enum_labels.push_back("7");
    def->enum_labels.push_back("8");
    def->enum_labels.push_back("9");

    def = this->add("extruder_clearance_height", coFloat);
    def->label = L("Height");
    def->full_label = L("Extruder clearance height");
    def->tooltip = L("Set this to the vertical distance between your nozzle tip and (usually) the X carriage rods. "
                   "In other words, this is the height of the clearance cylinder around your extruder, "
                   "and it represents the maximum depth the extruder can peek before colliding with "
                   "other printed objects.");
    def->sidetext = L("mm");
    def->min = 0;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloat(20));

    def = this->add("extruder_clearance_radius", coFloat);
    def->label = L("Radius");
    def->full_label = L("Extruder clearance radius");
    def->tooltip = L("Set this to the clearance radius around your extruder. "
                   "If the extruder is not centered, choose the largest value for safety. "
                   "This setting is used to check for collisions and to display the graphical preview "
                   "in the plater.");
    def->sidetext = L("mm");
    def->min = 0;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloat(20));

    def = this->add("extruder_colour", coStrings);
    def->label = L("Extruder Color");
    def->tooltip = L("This is only used in the Slic3r interface as a visual help.");
    def->gui_type = "color";
    // Empty string means no color assigned yet.
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionStrings{ "" });

    def = this->add("extruder_offset", coPoints);
    def->label = L("Extruder offset");
    def->tooltip = L("If your firmware doesn't handle the extruder displacement you need the G-code "
                   "to take it into account. This option lets you specify the displacement of each extruder "
                   "with respect to the first one. It expects positive coordinates (they will be subtracted "
                   "from the XY coordinate).");
    def->sidetext = L("mm");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionPoints { Vec2d(0,0) });

    def = this->add("extrusion_axis", coString);
    def->label = L("Extrusion axis");
    def->tooltip = L("Use this option to set the axis letter associated to your printer's extruder "
                   "(usually E but some printers use A).");
    def->set_default_value(new ConfigOptionString("E"));

    def = this->add("extrusion_multiplier", coFloats);
    def->label = L("Extrusion multiplier");
    def->tooltip = L("This factor changes the amount of flow proportionally. You may need to tweak "
                   "this setting to get nice surface finish and correct single wall widths. "
                   "Usual values are between 0.9 and 1.1. If you think you need to change this more, "
                   "check filament diameter and your firmware E steps.");
    def->mode = comSimple;
    def->set_default_value(new ConfigOptionFloats { 1. });
    
    def = this->add("extrusion_width", coFloatOrPercent);
    def->label = L("Default extrusion width");
    def->category = L("Extrusion Width");
    def->tooltip = L("Set this to a non-zero value to allow a manual extrusion width. "
                   "If left to zero, Slic3r derives extrusion widths from the nozzle diameter "
                   "(see the tooltips for perimeter extrusion width, infill extrusion width etc). "
                   "If expressed as percentage (for example: 105%), it will be computed over nozzle diameter.");
    def->sidetext = L("mm or %");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloatOrPercent(0, false));

    def = this->add("fan_always_on", coBools);
    def->label = L("Keep fan always on");
    def->tooltip = L("If this is enabled, fan will never be disabled and will be kept running at least "
                   "at its minimum speed. Useful for PLA, harmful for ABS.");
    def->mode = comSimple;
    def->set_default_value(new ConfigOptionBools{ false });

    def = this->add("fan_below_layer_time", coInts);
    def->label = L("Enable fan if layer print time is below");
    def->tooltip = L("If layer print time is estimated below this number of seconds, fan will be enabled "
                   "and its speed will be calculated by interpolating the minimum and maximum speeds.");
    def->sidetext = L("approximate seconds");
    def->width = 6;
    def->min = 0;
    def->max = 1000;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionInts { 60 });

    def = this->add("filament_colour", coStrings);
    def->label = L("Color");
    def->full_label = L("Filament color");
    def->tooltip = L("This is only used in the Slic3r interface as a visual help.");
    def->gui_type = "color";
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionStrings{ "#29B2B2" });

    def = this->add("filament_notes", coStrings);
    def->label = L("Filament notes");
    def->tooltip = L("You can put your notes regarding the filament here.");
    def->multiline = true;
    def->full_width = true;
    def->height = 13;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionStrings { "" });

    def = this->add("filament_max_volumetric_speed", coFloats);
    def->label = L("Max volumetric speed");
    def->tooltip = L("Maximum volumetric speed allowed for this filament. Limits the maximum volumetric "
                   "speed of a print to the minimum of print and filament volumetric speed. "
                   "Set to zero for no limit.");
    def->sidetext = L("mm³/s");
    def->min = 0;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloats { 0. });

    def = this->add("filament_max_wipe_tower_speed", coFloats);
    def->label = L("Max speed on the wipe tower");
    def->tooltip = L("This setting is used to set the maximum speed when extruding inside the wipe tower (use M220). In %, set 0 to disable and use the Filament type instead.");
    def->sidetext = L("% of mm/s");
    def->min = 0;
    def->max = 200;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloats { 0 });

    def = this->add("filament_loading_speed", coFloats);
    def->label = L("Loading speed");
    def->tooltip = L("Speed used for loading the filament on the wipe tower. ");
    def->sidetext = L("mm/s");
    def->min = 0;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloats { 28. });

    def = this->add("filament_loading_speed_start", coFloats);
    def->label = L("Loading speed at the start");
    def->tooltip = L("Speed used at the very beginning of loading phase. ");
    def->sidetext = L("mm/s");
    def->min = 0;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloats { 3. });

    def = this->add("filament_unloading_speed", coFloats);
    def->label = L("Unloading speed");
    def->tooltip = L("Speed used for unloading the filament on the wipe tower (does not affect "
                      " initial part of unloading just after ramming). ");
    def->sidetext = L("mm/s");
    def->min = 0;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloats { 90. });

    def = this->add("filament_unloading_speed_start", coFloats);
    def->label = L("Unloading speed at the start");
    def->tooltip = L("Speed used for unloading the tip of the filament immediately after ramming. ");
    def->sidetext = L("mm/s");
    def->min = 0;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloats { 100. });

    def = this->add("filament_toolchange_delay", coFloats);
    def->label = L("Delay after unloading");
    def->tooltip = L("Time to wait after the filament is unloaded. "
                   "May help to get reliable toolchanges with flexible materials "
                   "that may need more time to shrink to original dimensions. ");
    def->sidetext = L("s");
    def->min = 0;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloats { 0. });

    def = this->add("filament_cooling_moves", coInts);
    def->label = L("Number of cooling moves");
    def->tooltip = L("Filament is cooled by being moved back and forth in the "
                   "cooling tubes. Specify desired number of these moves.");
    def->max = 0;
    def->max = 20;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionInts { 4 });

    def = this->add("filament_cooling_initial_speed", coFloats);
    def->label = L("Speed of the first cooling move");
    def->tooltip = L("Cooling moves are gradually accelerating beginning at this speed. ");
    def->sidetext = L("mm/s");
    def->min = 0;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloats { 2.2f });

    def = this->add("filament_minimal_purge_on_wipe_tower", coFloats);
    def->label = L("Minimal purge on wipe tower");
    def->tooltip = L("After a tool change, the exact position of the newly loaded filament inside "
                     "the nozzle may not be known, and the filament pressure is likely not yet stable. "
                     "Before purging the print head into an infill or a sacrificial object, Slic3r will always prime "
                     "this amount of material into the wipe tower to produce successive infill or sacrificial object extrusions reliably.");
    def->sidetext = L("mm³");
    def->min = 0;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloats { 15.f });

    def = this->add("filament_cooling_final_speed", coFloats);
    def->label = L("Speed of the last cooling move");
    def->tooltip = L("Cooling moves are gradually accelerating towards this speed. ");
    def->sidetext = L("mm/s");
    def->min = 0;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloats { 3.4f });

    def = this->add("filament_load_time", coFloats);
    def->label = L("Filament load time");
    def->tooltip = L("Time for the printer firmware (or the Multi Material Unit 2.0) to load a new filament during a tool change (when executing the T code). This time is added to the total print time by the G-code time estimator.");
    def->sidetext = L("s");
    def->min = 0;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloats { 0.0f });

    def = this->add("filament_ramming_parameters", coStrings);
    def->label = L("Ramming parameters");
    def->tooltip = L("This string is edited by RammingDialog and contains ramming specific parameters.");
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionStrings { "120 100 6.6 6.8 7.2 7.6 7.9 8.2 8.7 9.4 9.9 10.0|"
	   " 0.05 6.6 0.45 6.8 0.95 7.8 1.45 8.3 1.95 9.7 2.45 10 2.95 7.6 3.45 7.6 3.95 7.6 4.45 7.6 4.95 7.6" });

    def = this->add("filament_unload_time", coFloats);
    def->label = L("Filament unload time");
    def->tooltip = L("Time for the printer firmware (or the Multi Material Unit 2.0) to unload a filament during a tool change (when executing the T code). This time is added to the total print time by the G-code time estimator.");
    def->sidetext = L("s");
    def->min = 0;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloats { 0.0f });

    def = this->add("filament_diameter", coFloats);
    def->label = L("Diameter");
    def->tooltip = L("Enter your filament diameter here. Good precision is required, so use a caliper "
                   "and do multiple measurements along the filament, then compute the average.");
    def->sidetext = L("mm");
    def->min = 0; 
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloats{ 1.75 });

    def = this->add("filament_density", coFloats);
    def->label = L("Density");
    def->tooltip = L("Enter your filament density here. This is only for statistical information. "
                   "A decent way is to weigh a known length of filament and compute the ratio "
                   "of the length to volume. Better is to calculate the volume directly through displacement.");
    def->sidetext = L("g/cm³");
    def->min = 0;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloats{ 0. });

    def = this->add("filament_type", coStrings);
    def->label = L("Filament type");
    def->tooltip = L("The filament material type for use in custom G-codes.");
    def->gui_type = "f_enum_open";
    def->gui_flags = "show_value";
    def->enum_values.push_back("PLA");
    def->enum_values.push_back("ABS");
    def->enum_values.push_back("PET");
    def->enum_values.push_back("FLEX");
    def->enum_values.push_back("HIPS");
    def->enum_values.push_back("EDGE");
    def->enum_values.push_back("NGEN");
    def->enum_values.push_back("NYLON");
    def->enum_values.push_back("PVA");
    def->enum_values.push_back("PC");
    def->enum_values.push_back("PP");
    def->enum_values.push_back("PEI");
    def->enum_values.push_back("PEEK");
    def->enum_values.push_back("PEKK");
    def->enum_values.push_back("POM");
    def->enum_values.push_back("PSU");
    def->enum_values.push_back("PVDF");
    def->enum_values.push_back("SCAFF");
    def->enum_values.push_back("other0");
    def->enum_values.push_back("other1");
    def->enum_values.push_back("other2");
    def->enum_values.push_back("other3");
    def->enum_values.push_back("other4");
    def->enum_values.push_back("other5");
    def->enum_values.push_back("other6");
    def->enum_values.push_back("other7");
    def->enum_values.push_back("other8");
    def->enum_values.push_back("other9");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionStrings { "PLA" });

    def = this->add("filament_soluble", coBools);
    def->label = L("Soluble material");
    def->tooltip = L("Soluble material is most likely used for a soluble support.");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionBools { false });

    def = this->add("filament_cost", coFloats);
    def->label = L("Cost");
    def->tooltip = L("Enter your filament cost per kg here. This is only for statistical information.");
    def->sidetext = L("money/kg");
    def->min = 0;
    def->set_default_value(new ConfigOptionFloats { 0. });
    
    def = this->add("filament_settings_id", coStrings);
    def->set_default_value(new ConfigOptionStrings { "" });
    def->cli = ConfigOptionDef::nocli;

    def = this->add("fill_angle", coFloat);
    def->label = L("Fill");
    def->full_label = L("Fill angle");
    def->category = L("Infill");
    def->tooltip = L("Default base angle for infill orientation. Cross-hatching will be applied to this. "
                   "Bridges will be infilled using the best direction Slic3r can detect, so this setting "
                   "does not affect them.");
    def->sidetext = L("°");
    def->min = 0;
    def->max = 360;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloat(45));

    def = this->add("fill_density", coPercent);
    def->gui_type = "f_enum_open";
    def->gui_flags = "show_value";
    def->label = L("Fill density");
    def->category = L("Infill");
    def->tooltip = L("Density of internal infill, expressed in the range 0% - 100%.");
    def->sidetext = L("%");
    def->min = 0;
    def->max = 100;
    /*
    def->enum_values.push_back("0");
    def->enum_values.push_back("5");
    def->enum_values.push_back("10");
    def->enum_values.push_back("15");
    def->enum_values.push_back("20");
    def->enum_values.push_back("25");
    def->enum_values.push_back("30");
    def->enum_values.push_back("40");
    def->enum_values.push_back("50");
    def->enum_values.push_back("60");
    def->enum_values.push_back("70");
    def->enum_values.push_back("80");
    def->enum_values.push_back("90");
    def->enum_values.push_back("100");
    def->enum_labels.push_back("0%");
    def->enum_labels.push_back("5%");
    def->enum_labels.push_back("10%");
    def->enum_labels.push_back("15%");
    def->enum_labels.push_back("20%");
    def->enum_labels.push_back("25%");
    def->enum_labels.push_back("30%");
    def->enum_labels.push_back("40%");
    def->enum_labels.push_back("50%");
    def->enum_labels.push_back("60%");
    def->enum_labels.push_back("70%");
    def->enum_labels.push_back("80%");
    def->enum_labels.push_back("90%");
    def->enum_labels.push_back("100%");
    def->set_default_value(new ConfigOptionPercent(20);
    */
    def->enum_values.push_back("0");
    def->enum_values.push_back("4");
    def->enum_values.push_back("5.5");
    def->enum_values.push_back("7.5");
    def->enum_values.push_back("10");
    def->enum_values.push_back("13");
    def->enum_values.push_back("18");
    def->enum_values.push_back("23");
    def->enum_values.push_back("31");
    def->enum_values.push_back("42");
    def->enum_values.push_back("55");
    def->enum_values.push_back("75");
    def->enum_values.push_back("100");
    def->enum_labels.push_back("0");
    def->enum_labels.push_back("4");
    def->enum_labels.push_back("5.5");
    def->enum_labels.push_back("7.5");
    def->enum_labels.push_back("10");
    def->enum_labels.push_back("13");
    def->enum_labels.push_back("18");
    def->enum_labels.push_back("23");
    def->enum_labels.push_back("31");
    def->enum_labels.push_back("42");
    def->enum_labels.push_back("55");
    def->enum_labels.push_back("75");
    def->enum_labels.push_back("100");
    def->set_default_value(new ConfigOptionPercent(18));

    def = this->add("fill_pattern", coEnum);
    def->label = L("Pattern");
    def->full_label = L("Fill pattern");
    def->category = L("Infill");
    def->tooltip = L("Fill pattern for general low-density infill.");
    def->enum_keys_map = &ConfigOptionEnum<InfillPattern>::get_enum_values();
    def->enum_values.push_back("rectilinear");
    def->enum_values.push_back("grid");
    def->enum_values.push_back("triangles");
    def->enum_values.push_back("stars");
    def->enum_values.push_back("cubic");
    def->enum_values.push_back("line");
    def->enum_values.push_back("concentric");
    def->enum_values.push_back("honeycomb");
    def->enum_values.push_back("3dhoneycomb");
    def->enum_values.push_back("gyroid");
    def->enum_values.push_back("hilbertcurve");
    def->enum_values.push_back("archimedeanchords");
    def->enum_values.push_back("octagramspiral");
    def->enum_values.push_back("scatteredrectilinear"); 
    def->enum_labels.push_back(L("Rectilinear"));
    def->enum_labels.push_back(L("Grid"));
    def->enum_labels.push_back(L("Triangles"));
    def->enum_labels.push_back(L("Stars"));
    def->enum_labels.push_back(L("Cubic"));
    def->enum_labels.push_back(L("Line"));
    def->enum_labels.push_back(L("Concentric"));
    def->enum_labels.push_back(L("Honeycomb"));
    def->enum_labels.push_back(L("3D Honeycomb"));
    def->enum_labels.push_back(L("Gyroid"));
    def->enum_labels.push_back(L("Hilbert Curve"));
    def->enum_labels.push_back(L("Archimedean Chords"));
    def->enum_labels.push_back(L("Octagram Spiral"));
    def->enum_labels.push_back(L("Scattered Rectilinear"));
    def->set_default_value( new ConfigOptionEnum<InfillPattern>(ipStars));

    def = this->add("first_layer_acceleration", coFloat);
    def->label = L("First layer");
    def->full_label = L("First layer acceleration");
    def->tooltip = L("This is the acceleration your printer will use for first layer. Set zero "
                   "to disable acceleration control for first layer.");
    def->sidetext = L("mm/s²");
    def->min = 0;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloat(0));

    def = this->add("first_layer_bed_temperature", coInts);
    def->label = L("First layer");
    def->full_label = L("First layer bed temperature");
    def->tooltip = L("Heated build plate temperature for the first layer. Set this to zero to disable "
                   "bed temperature control commands in the output.");
    def->sidetext = L("°C");
    def->max = 0;
    def->max = 300;
    def->set_default_value(new ConfigOptionInts { 0 });

    def = this->add("first_layer_extrusion_width", coFloatOrPercent);
    def->label = L("First layer");
    def->full_label = L("First layer width");
    def->category = L("Extrusion Width");
    def->tooltip = L("Set this to a non-zero value to set a manual extrusion width for first layer. "
                   "You can use this to force fatter extrudates for better adhesion. If expressed "
                   "as percentage (for example 140%) it will be computed over the nozzle diameter "
                   "of the nozzle used for the type of extrusion. "
                   "If set to zero, it will use the default extrusion width.");
    def->sidetext = L("mm or %");
    def->ratio_over = "first_layer_height";
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloatOrPercent(140, true));

    def = this->add("first_layer_height", coFloatOrPercent);
    def->label = L("First layer height");
    def->category = L("Layers and Perimeters");
    def->tooltip = L("When printing with very low layer heights, you might still want to print a thicker "
                   "bottom layer to improve adhesion and tolerance for non perfect build plates. "
                   "This can be expressed as an absolute value or as a percentage (for example: 150%) "
                   "over the default layer height.");
    def->sidetext = L("mm or %");
    def->ratio_over = "layer_height";
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloatOrPercent(0.2, false));

    def = this->add("first_layer_speed", coFloatOrPercent);
    def->label = L("Default");
    def->full_label = L("Default first layer speed");
    def->tooltip = L("If expressed as absolute value in mm/s, this speed will be applied to all the print moves "
                   "but infill of the first layer, it can be overwrite by the 'default' (default depends of the type of the path) "
                   "speed if it's lower than that. If expressed as a percentage "
                   "(for example: 40%) it will scale the 'default' speeds.");
    def->sidetext = L("mm/s or %");
    def->min = 0;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloatOrPercent(30, false));
    
    def = this->add("first_layer_infill_speed", coFloatOrPercent);
    def->label = L("Infill");
    def->full_label = L("Infill first layer speed");
    def->tooltip = L("If expressed as absolute value in mm/s, this speed will be applied to infill moves "
                   "of the first layer, it can be overwrite by the 'default' (solid infill or infill if not bottom) "
                   "speed if it's lower than that. If expressed as a percentage "
                   "(for example: 40%) it will scale the 'default' speed.");
    def->sidetext = L("mm/s or %");
    def->min = 0;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloatOrPercent(30, false));
    
    def = this->add("first_layer_temperature", coInts);
    def->label = L("First layer");
    def->full_label = L("First layer temperature");
    def->tooltip = L("Extruder temperature for first layer. If you want to control temperature manually "
                   "during print, set this to zero to disable temperature control commands in the output file.");
    def->sidetext = L("°C");
    def->min = 0;
    def->max = max_temp;
    def->set_default_value(new ConfigOptionInts { 200 });

    def = this->add("gap_fill", coBool);
    def->label = L("Gap fill");
    def->category = L("Advanced");
    def->tooltip = L("Enable gap fill algorithm. It will extrude small lines between perimeters "
        "when there is not enough space for another perimeter or an infill.");
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionBool(true));

    def = this->add("gap_fill_speed", coFloat);
    def->label = L("Gap fill");
    def->full_label = L("Gap fill speed");
    def->category = L("Speed");
    def->tooltip = L("Speed for filling small gaps using short zigzag moves. Keep this reasonably low "
        "to avoid too much shaking and resonance issues. Set zero to disable gaps filling.");
    def->sidetext = L("mm/s");
    def->min = 0;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloat(20));

    def = this->add("gcode_comments", coBool);
    def->label = L("Verbose G-code");
    def->tooltip = L("Enable this to get a commented G-code file, with each line explained by a descriptive text. "
        "If you print from SD card, the additional weight of the file could make your firmware "
        "slow down.");
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionBool(0));

    def = this->add("gcode_flavor", coEnum);
    def->label = L("G-code flavor");
    def->tooltip = L("Some G/M-code commands, including temperature control and others, are not universal. "
                   "Set this option to your printer's firmware to get a compatible output. "
                   "The \"No extrusion\" flavor prevents Slic3r from exporting any extrusion value at all.");
    def->enum_keys_map = &ConfigOptionEnum<GCodeFlavor>::get_enum_values();
    def->enum_values.push_back("reprap");
    def->enum_values.push_back("repetier");
    def->enum_values.push_back("teacup");
    def->enum_values.push_back("makerware");
    def->enum_values.push_back("marlin");
    def->enum_values.push_back("sailfish");
    def->enum_values.push_back("mach3");
    def->enum_values.push_back("machinekit");
    def->enum_values.push_back("smoothie");
    def->enum_values.push_back("no-extrusion");
    def->enum_labels.push_back("RepRap/Sprinter");
    def->enum_labels.push_back("Repetier");
    def->enum_labels.push_back("Teacup");
    def->enum_labels.push_back("MakerWare (MakerBot)");
    def->enum_labels.push_back("Marlin");
    def->enum_labels.push_back("Sailfish (MakerBot)");
    def->enum_labels.push_back("Mach3/LinuxCNC");
    def->enum_labels.push_back("Machinekit");
    def->enum_labels.push_back("Smoothie");
    def->enum_labels.push_back(L("No extrusion"));
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionEnum<GCodeFlavor>(gcfRepRap));

    def = this->add("gcode_label_objects", coBool);
    def->label = L("Label objects");
    def->tooltip = L("Enable this to add comments into the G-Code labeling print moves with what object they belong to,"
                   " which is useful for the Octoprint CancelObject plugin. This settings is NOT compatible with "
                   "Single Extruder Multi Material setup and Wipe into Object / Wipe into Infill.");
    def->aliases = { "label_printed_objects" };
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionBool(1));

    def = this->add("high_current_on_filament_swap", coBool);
    def->label = L("High extruder current on filament swap");
    def->tooltip = L("It may be beneficial to increase the extruder motor current during the filament exchange"
                   " sequence to allow for rapid ramming feed rates and to overcome resistance when loading"
                   " a filament with an ugly shaped tip.");
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionBool(0));

    def = this->add("infill_acceleration", coFloat);
    def->label = L("Infill");
    def->full_label = L("Infill acceleration");
    def->tooltip = L("This is the acceleration your printer will use for infill. Set zero to disable "
                   "acceleration control for infill.");
    def->sidetext = L("mm/s²");
    def->min = 0;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloat(0));

    def = this->add("infill_every_layers", coInt);
    def->label = L("Combine infill every");
    def->category = L("Infill");
    def->tooltip = L("This feature allows to combine infill and speed up your print by extruding thicker "
                   "infill layers while preserving thin perimeters, thus accuracy.");
    def->sidetext = L("layers");
    def->full_label = L("Combine infill every n layers");
    def->min = 1;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionInt(1));

    def = this->add("infill_dense", coBool);
    def->label = ("");
    def->full_label = ("Dense infill layer");
    def->category = L("Infill");
    def->tooltip = L("Enables the creation of a support layer under the first solid layer. This allows you to use a lower infill ratio without compromising the top quality."
        " The dense infill is laid out with a 50% infill density.");
    def->mode = comSimple;
    def->set_default_value(new ConfigOptionBool(false));

    def = this->add("infill_not_connected", coBool);
    def->label = ("Do not connect infill lines to each other");
    def->category = L("Infill");
    def->tooltip = L("If checked, the infill algorithm will try to not connect the lines near the infill. Can be useful for art or with high infill/perimeter overlap.");
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionBool(false));
    
    def = this->add("infill_dense_algo", coEnum);
    def->label = L("Algorithm");
    def->full_label = ("Dense infill algorithm");
    def->tooltip = L("Choose the way the dense layer is lay out."
        " The automatic option let it try to draw the smallest surface with only strait lines inside the sparse infill."
        " The anchored just enlarge a bit (by 'Default infill margin') the surfaces that need a better support.");
    def->enum_keys_map = &ConfigOptionEnum<DenseInfillAlgo>::get_enum_values();
    def->enum_values.push_back("automatic");
    def->enum_values.push_back("autosmall");
    def->enum_values.push_back("enlarged");
    def->enum_labels.push_back(L("Automatic"));
    def->enum_labels.push_back(L("Automatic, only for small areas"));
    def->enum_labels.push_back(L("Anchored"));
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionEnum<DenseInfillAlgo>(dfaAutomatic));

    def = this->add("infill_extruder", coInt);
    def->label = L("Infill extruder");
    def->category = L("Extruders");
    def->tooltip = L("The extruder to use when printing infill.");
    def->min = 1;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionInt(1));

    def = this->add("infill_extrusion_width", coFloatOrPercent);
    def->label = L("Infill");
    def->category = L("Extrusion Width");
    def->tooltip = L("Set this to a non-zero value to set a manual extrusion width for infill. "
                   "If left zero, default extrusion width will be used if set, otherwise 1.125 x nozzle diameter will be used. "
                   "You may want to use fatter extrudates to speed up the infill and make your parts stronger. "
                   "If expressed as percentage (for example 110%) it will be computed over nozzle diameter.");
    def->sidetext = L("mm or %");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloatOrPercent(0, false));

    def = this->add("infill_first", coBool);
    def->label = L("Infill before perimeters");
    def->category = L("Infill");
    def->tooltip = L("This option will switch the print order of perimeters and infill, making the latter first.");
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionBool(false));

    def = this->add("infill_only_where_needed", coBool);
    def->label = L("Only infill where needed");
    def->category = L("Infill");
    def->tooltip = L("This option will limit infill to the areas actually needed for supporting ceilings "
                   "(it will act as internal support material). If enabled, slows down the G-code generation "
                   "due to the multiple checks involved.");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionBool(false));

    def = this->add("infill_overlap", coFloatOrPercent);
    def->label = L("Infill/perimeters overlap");
    def->category = L("Advanced");
    def->tooltip = L("This setting applies an additional overlap between infill and perimeters for better bonding. "
                   "Theoretically this shouldn't be needed, but backlash might cause gaps. If expressed "
                   "as percentage (example: 15%) it is calculated over perimeter extrusion width.");
    def->sidetext = L("mm or %");
    def->ratio_over = "perimeter_extrusion_width";
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloatOrPercent(25, true));

    def = this->add("infill_speed", coFloat);
    def->label = L("Sparse");
    def->full_label = ("Sparse infill speed");
    def->category = L("Speed");
    def->tooltip = L("Speed for printing the internal fill. Set to zero for auto.");
    def->sidetext = L("mm/s");
    def->aliases = { "print_feed_rate", "infill_feed_rate" };
    def->min = 0;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloat(80));

    def = this->add("inherits", coString);
    def->label = L("Inherits profile");
    def->tooltip = L("Name of the profile, from which this profile inherits.");
    def->full_width = true;
    def->height = 5;
    def->set_default_value(new ConfigOptionString());
    def->cli = ConfigOptionDef::nocli;

    // The following value is to be stored into the project file (AMF, 3MF, Config ...)
    // and it contains a sum of "inherits" values over the print and filament profiles.
    def = this->add("inherits_cummulative", coStrings);
    def->set_default_value(new ConfigOptionStrings());
    def->cli = ConfigOptionDef::nocli;

    def = this->add("interface_shells", coBool);
    def->label = L("Interface shells");
    def->tooltip = L("Force the generation of solid shells between adjacent materials/volumes. "
                   "Useful for multi-extruder prints with translucent materials or manual soluble "
                   "support material.");
    def->category = L("Layers and Perimeters");
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionBool(false));

    def = this->add("layer_gcode", coString);
    def->label = L("After layer change G-code");
    def->tooltip = L("This custom code is inserted at every layer change, right after the Z move "
                   "and before the extruder moves to the first layer point. Note that you can use "
                   "placeholder variables for all Slic3r settings as well as [layer_num] and [layer_z].");
    def->cli = "after-layer-gcode|layer-gcode";
    def->multiline = true;
    def->full_width = true;
    def->height = 5;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionString(""));

    def = this->add("exact_last_layer_height", coBool);
    def->label = L("Exact last layer height");
    def->category = L("Layers and Perimeters");
    def->tooltip = L("This setting controls the height of last object layers to put the last layer at the exact highest height possible. Experimental.");
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionBool(false));

    def = this->add("remaining_times", coBool);
    def->label = L("Supports remaining times");
    def->category = L("Firmware");
    def->tooltip = L("Emit M73 P[percent printed] R[remaining time in minutes] at 1 minute"
                     " intervals into the G-code to let the firmware show accurate remaining time."
                     " As of now only the Prusa i3 MK3 firmware recognizes M73."
                     " Also the i3 MK3 firmware supports M73 Qxx Sxx for the silent mode.");
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionBool(false));

	def = this->add("silent_mode", coBool);
    def->label = L("Supports stealth mode");
    def->category = L("Firmware");
    def->tooltip = L("The firmware supports stealth mode");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionBool(true));

    def = this->add("fan_speedup_time", coFloat);
    def->label = L("fan startup delay");
    def->category = L("Firmware");
    def->tooltip = L("Move the M106 in the past by at least this delay (in seconds, you can use decimals) and add the 'D' option to it to tell to the firware when the fan have to be at this speed."
        " It assume infinite acceleration for this time estimation, and only takes into account G1 and G0 moves. Use 0 to deactivate, negative to remove the 'D' option.");
    def->sidetext = L("s");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloat(0));

	const int machine_limits_opt_width = 70;
	{
		struct AxisDefault {
			std::string         name;
			std::vector<double> max_feedrate;
			std::vector<double> max_acceleration;
			std::vector<double> max_jerk;
		};
		std::vector<AxisDefault> axes {
			// name, max_feedrate,  max_acceleration, max_jerk
			{ "x", { 500., 200. }, {  9000., 1000. }, { 10. , 10.  } },
			{ "y", { 500., 200. }, {  9000., 1000. }, { 10. , 10.  } },
			{ "z", {  12.,  12. }, {   500.,  200. }, {  0.2,  0.4 } },
			{ "e", { 120., 120. }, { 10000., 5000. }, {  2.5,  2.5 } }
		};
		for (const AxisDefault &axis : axes) {
			std::string axis_upper = boost::to_upper_copy<std::string>(axis.name);
			// Add the machine feedrate limits for XYZE axes. (M203)
			def = this->add("machine_max_feedrate_" + axis.name, coFloats);
			def->full_label = (boost::format("Maximum feedrate %1%") % axis_upper).str();
			(void)L("Maximum feedrate X");
			(void)L("Maximum feedrate Y");
			(void)L("Maximum feedrate Z");
			(void)L("Maximum feedrate E");
			def->category = L("Machine limits");
			def->tooltip  = (boost::format("Maximum feedrate of the %1% axis") % axis_upper).str();
			(void)L("Maximum feedrate of the X axis");
			(void)L("Maximum feedrate of the Y axis");
			(void)L("Maximum feedrate of the Z axis");
			(void)L("Maximum feedrate of the E axis");
			def->sidetext = L("mm/s");
			def->min = 0;
			def->width = machine_limits_opt_width;
            def->mode = comAdvanced;
			def->set_default_value(new ConfigOptionFloats(axis.max_feedrate));
			// Add the machine acceleration limits for XYZE axes (M201)
			def = this->add("machine_max_acceleration_" + axis.name, coFloats);
			def->full_label = (boost::format("Maximum acceleration %1%") % axis_upper).str();
			(void)L("Maximum acceleration X");
			(void)L("Maximum acceleration Y");
			(void)L("Maximum acceleration Z");
			(void)L("Maximum acceleration E");
			def->category = L("Machine limits");
			def->tooltip  = (boost::format("Maximum acceleration of the %1% axis") % axis_upper).str();
			(void)L("Maximum acceleration of the X axis");
			(void)L("Maximum acceleration of the Y axis");
			(void)L("Maximum acceleration of the Z axis");
			(void)L("Maximum acceleration of the E axis");
			def->sidetext = L("mm/s²");
			def->min = 0;
			def->width = machine_limits_opt_width;
            def->mode = comAdvanced;
			def->set_default_value(new ConfigOptionFloats(axis.max_acceleration));
			// Add the machine jerk limits for XYZE axes (M205)
			def = this->add("machine_max_jerk_" + axis.name, coFloats);
			def->full_label = (boost::format("Maximum jerk %1%") % axis_upper).str();
			(void)L("Maximum jerk X");
			(void)L("Maximum jerk Y");
			(void)L("Maximum jerk Z");
			(void)L("Maximum jerk E");
			def->category = L("Machine limits");
			def->tooltip  = (boost::format("Maximum jerk of the %1% axis") % axis_upper).str();
			(void)L("Maximum jerk of the X axis");
			(void)L("Maximum jerk of the Y axis");
			(void)L("Maximum jerk of the Z axis");
			(void)L("Maximum jerk of the E axis");
			def->sidetext = L("mm/s");
			def->min = 0;
			def->width = machine_limits_opt_width;
            def->mode = comAdvanced;
			def->set_default_value(new ConfigOptionFloats(axis.max_jerk));
		}
	}

    // M205 S... [mm/sec]
    def = this->add("machine_min_extruding_rate", coFloats);
    def->full_label = L("Minimum feedrate when extruding");
    def->category = L("Machine limits");
    def->tooltip = L("Minimum feedrate when extruding (M205 S)");
    def->sidetext = L("mm/s");
    def->min = 0;
    def->width = machine_limits_opt_width;
    def->mode = comAdvanced;
	def->set_default_value(new ConfigOptionFloats{ 0., 0. });

    // M205 T... [mm/sec]
    def = this->add("machine_min_travel_rate", coFloats);
    def->full_label = L("Minimum travel feedrate");
    def->category = L("Machine limits");
    def->tooltip = L("Minimum travel feedrate (M205 T)");
    def->sidetext = L("mm/s");
    def->min = 0;
    def->width = machine_limits_opt_width;
    def->mode = comAdvanced;
	def->set_default_value(new ConfigOptionFloats{ 0., 0. });

    // M204 S... [mm/sec^2]
    def = this->add("machine_max_acceleration_extruding", coFloats);
    def->full_label = L("Maximum acceleration when extruding");
    def->category = L("Machine limits");
    def->tooltip = L("Maximum acceleration when extruding (M204 S)");
    def->sidetext = L("mm/s²");
    def->min = 0;
    def->width = machine_limits_opt_width;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloats{ 1500., 1250. });

    // M204 T... [mm/sec^2]
    def = this->add("machine_max_acceleration_retracting", coFloats);
    def->full_label = L("Maximum acceleration when retracting");
    def->category = L("Machine limits");
    def->tooltip = L("Maximum acceleration when retracting (M204 T)");
    def->sidetext = L("mm/s²");
    def->min = 0;
    def->width = machine_limits_opt_width;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloats{ 1500., 1250. });

    def = this->add("max_fan_speed", coInts);
    def->label = L("Max");
    def->full_label = ("Max fan speed");
    def->tooltip = L("This setting represents the maximum speed of your fan.");
    def->sidetext = L("%");
    def->min = 0;
    def->max = 100;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionInts { 100 });

    def = this->add("max_layer_height", coFloats);
    def->label = L("Max");
    def->full_label = ("Max layer height");
    def->tooltip = L("This is the highest printable layer height for this extruder, used to cap "
                   "the variable layer height and support layer height. Maximum recommended layer height "
                   "is 75% of the extrusion width to achieve reasonable inter-layer adhesion. "
                   "If set to 0, layer height is limited to 75% of the nozzle diameter.");
    def->sidetext = L("mm");
    def->min = 0;
    def->mode = comSimple;
    def->set_default_value(new ConfigOptionFloats { 0. });

    def = this->add("max_print_speed", coFloat);
    def->label = L("Max print speed");
    def->tooltip = L("When setting other speed settings to 0 Slic3r will autocalculate the optimal speed "
                   "in order to keep constant extruder pressure. This experimental setting is used "
                   "to set the highest print speed you want to allow.");
    def->sidetext = L("mm/s");
    def->min = 1;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloat(80));

    def = this->add("max_volumetric_speed", coFloat);
    def->label = L("Max volumetric speed");
    def->tooltip = L("This experimental setting is used to set the maximum volumetric speed your "
                   "extruder supports.");
    def->sidetext = L("mm³/s");
    def->min = 0;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloat(0));

#ifdef HAS_PRESSURE_EQUALIZER
    def = this->add("max_volumetric_extrusion_rate_slope_positive", coFloat);
    def->label = L("Max volumetric slope positive");
    def->tooltip = L("This experimental setting is used to limit the speed of change in extrusion rate. "
                   "A value of 1.8 mm³/s² ensures, that a change from the extrusion rate "
                   "of 1.8 mm³/s (0.45mm extrusion width, 0.2mm extrusion height, feedrate 20 mm/s) "
                   "to 5.4 mm³/s (feedrate 60 mm/s) will take at least 2 seconds.");
    def->sidetext = L("mm³/s²");
    def->min = 0;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloat(0);
    def->set_default_value(new ConfigOptionFloat(0));

    def = this->add("max_volumetric_extrusion_rate_slope_negative", coFloat);
    def->label = L("Max volumetric slope negative");
    def->tooltip = L("This experimental setting is used to limit the speed of change in extrusion rate. "
                   "A value of 1.8 mm³/s² ensures, that a change from the extrusion rate "
                   "of 1.8 mm³/s (0.45mm extrusion width, 0.2mm extrusion height, feedrate 20 mm/s) "
                   "to 5.4 mm³/s (feedrate 60 mm/s) will take at least 2 seconds.");
    def->sidetext = L("mm³/s²");
    def->min = 0;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloat(0);
    def->set_default_value(new ConfigOptionFloat(0));
#endif /* HAS_PRESSURE_EQUALIZER */

    def = this->add("min_fan_speed", coInts);
    def->label = L("Min");
    def->full_label = ("Min fan speed");
    def->tooltip = L("This setting represents the minimum PWM your fan needs to work.");
    def->sidetext = L("%");
    def->min = 0;
    def->max = 100;
    def->mode = comSimple;
    def->set_default_value(new ConfigOptionInts { 35 });

    def = this->add("min_layer_height", coFloats);
    def->label = L("Min");
    def->full_label = ("Min layer height");
    def->tooltip = L("This is the lowest printable layer height for this extruder and limits "
                   "the resolution for variable layer height. Typical values are between 0.05 mm and 0.1 mm.");
    def->sidetext = L("mm");
    def->min = 0;
    def->mode = comSimple;
    def->set_default_value(new ConfigOptionFloats { 0.07 });

    def = this->add("min_print_speed", coFloats);
    def->label = L("Min print speed");
    def->tooltip = L("Slic3r will not scale speed down below this speed.");
    def->sidetext = L("mm/s");
    def->min = 0;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloats { 10. });

    def = this->add("min_skirt_length", coFloat);
    def->label = L("Minimal filament extrusion length");
    def->tooltip = L("Generate no less than the number of skirt loops required to consume "
                   "the specified amount of filament on the bottom layer. For multi-extruder machines, "
                   "this minimum applies to each extruder.");
    def->sidetext = L("mm");
    def->min = 0;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloat(0));

    def = this->add("notes", coString);
    def->label = L("Configuration notes");
    def->tooltip = L("You can put here your personal notes. This text will be added to the G-code "
                   "header comments.");
    def->multiline = true;
    def->full_width = true;
    def->height = 13;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionString(""));

    def = this->add("nozzle_diameter", coFloats);
    def->label = L("Nozzle diameter");
    def->tooltip = L("This is the diameter of your extruder nozzle (for example: 0.5, 0.35 etc.)");
    def->sidetext = L("mm");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloats{ 0.4 });

    def = this->add("host_type", coEnum);
    def->label = L("Host Type");
    def->tooltip = L("Slic3r can upload G-code files to a printer host. This field must contain "
                   "the kind of the host.");
    def->enum_keys_map = &ConfigOptionEnum<PrintHostType>::get_enum_values();
    def->enum_values.push_back("octoprint");
    def->enum_values.push_back("duet");
    def->enum_labels.push_back("OctoPrint");
    def->enum_labels.push_back("Duet");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionEnum<PrintHostType>(htOctoPrint));

    def = this->add("printhost_apikey", coString);
    def->label = L("API Key / Password");
    def->tooltip = L("Slic3r can upload G-code files to a printer host. This field should contain "
                   "the API Key or the password required for authentication.");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionString(""));
    
    def = this->add("printhost_cafile", coString);
    def->label = L("HTTPS CA File");
    def->tooltip = "Custom CA certificate file can be specified for HTTPS OctoPrint connections, in crt/pem format. "
                   "If left blank, the default OS CA certificate repository is used.";
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionString(""));

    def = this->add("print_host", coString);
    def->label = L("Hostname, IP or URL");
    def->tooltip = L("Slic3r can upload G-code files to a printer host. This field should contain "
                   "the hostname, IP address or URL of the printer host instance.");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionString(""));

    def = this->add("only_retract_when_crossing_perimeters", coBool);
    def->label = L("Only retract when crossing perimeters");
    def->tooltip = L("Disables retraction when the travel path does not exceed the upper layer's perimeters "
                   "(and thus any ooze will be probably invisible).");
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionBool(true));

    def = this->add("ooze_prevention", coBool);
    def->label = L("Enable");
    def->tooltip = L("This option will drop the temperature of the inactive extruders to prevent oozing. "
                   "It will enable a tall skirt automatically and move extruders outside such "
                   "skirt when changing temperatures.");
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionBool(false));

    def = this->add("output_filename_format", coString);
    def->label = L("Output filename format");
    def->tooltip = L("You can use all configuration options as variables inside this template. "
                   "For example: [layer_height], [fill_density] etc. You can also use [timestamp], "
                   "[year], [month], [day], [hour], [minute], [second], [version], [input_filename], "
                   "[input_filename_base].");
    def->full_width = true;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionString("[input_filename_base].gcode"));

    def = this->add("overhangs", coBool);
    def->label = L("Detect bridging perimeters");
    def->category = L("Layers and Perimeters");
    def->tooltip = L("Experimental option to adjust flow for overhangs (bridge flow will be used), "
        "to apply bridge speed to them and enable fan.");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionBool(true));
    
    def = this->add("no_perimeter_unsupported_algo", coEnum);
    def->label = L("No perimeters on bridge areas");
    def->tooltip = L("Experimental option to remove perimeters where there is nothing under it and where a bridged infill should be better. "
        "\n * Remove perimeters: remove the unsupported perimeter, let the bridge area as-is."
        "\n * Keep only bridges: remove the unsupported perimeter, kep only bridges that end in solid area."
        "\n * Keep bridges and overhangs: remove the unsupported perimeter, keep only bridges that end in solid area, fill the rest with overhang perimeters+bridges."
        "\n * Fill the voids with bridges: remove the unsupported perimeter, draw bridges over the whole hole. !! can lead to problems with overhangs shape like  /\\, consider carefully before using this option!"
		"\n!!Computationally intensive!!. ");
    def->enum_keys_map = &ConfigOptionEnum<NoPerimeterUnsupportedAlgo>::get_enum_values();
    def->enum_values.push_back("none");
    def->enum_values.push_back("noperi");
    def->enum_values.push_back("bridges");
    def->enum_values.push_back("bridgesoverhangs");
    def->enum_values.push_back("filled");
    def->enum_labels.push_back(L("Disabled"));
    def->enum_labels.push_back(L("Remove perimeters"));
    def->enum_labels.push_back(L("Keep only bridges"));
    def->enum_labels.push_back(L("Keep bridges and overhangs"));
    def->enum_labels.push_back(L("Fill the voids with bridges"));
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionEnum<NoPerimeterUnsupportedAlgo>(npuaNone));

    def = this->add("parking_pos_retraction", coFloat);
    def->label = L("Filament parking position");
    def->tooltip = L("Distance of the extruder tip from the position where the filament is parked "
                      "when unloaded. This should match the value in printer firmware. ");
    def->sidetext = L("mm");
    def->min = 0;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloat(92.f));

    def = this->add("extra_loading_move", coFloat);
    def->label = L("Extra loading distance");
    def->tooltip = L("When set to zero, the distance the filament is moved from parking position during load "
                      "is exactly the same as it was moved back during unload. When positive, it is loaded further, "
                      " if negative, the loading move is shorter than unloading. ");
    def->sidetext = L("mm");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloat(-2.f));

    def = this->add("perimeter_acceleration", coFloat);
    def->label = L("Perimeters");
    def->full_label = ("Perimeter acceleration");
    def->tooltip = L("This is the acceleration your printer will use for perimeters. "
                   "A high value like 9000 usually gives good results if your hardware is up to the job. "
                   "Set zero to disable acceleration control for perimeters.");
    def->sidetext = L("mm/s²");
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloat(0));

    def = this->add("perimeter_extruder", coInt);
    def->label = L("Perimeter extruder");
    def->category = L("Extruders");
    def->tooltip = L("The extruder to use when printing perimeters and brim. First extruder is 1.");
    def->aliases = { "perimeters_extruder" };
    def->min = 1;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionInt(1));

    def = this->add("perimeter_extrusion_width", coFloatOrPercent);
    def->label = L("Perimeters");
    def->full_label = ("Perimeter width");
    def->category = L("Extrusion Width");
    def->tooltip = L("Set this to a non-zero value to set a manual extrusion width for perimeters. "
                   "You may want to use thinner extrudates to get more accurate surfaces. "
                   "If left zero, default extrusion width will be used if set, otherwise 1.125 x nozzle diameter will be used. "
                   "If expressed as percentage (for example 105%) it will be computed over nozzle diameter.");
    def->sidetext = L("mm or %");
    def->aliases = { "perimeters_extrusion_width" };
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloatOrPercent(0, false));

    def = this->add("perimeter_speed", coFloat);
    def->label = L("Default");
    def->full_label = ("Default speed");
    def->category = L("Speed");
    def->tooltip = L("Speed for perimeters (contours, aka vertical shells). Set to zero for auto.");
    def->sidetext = L("mm/s");
    def->aliases = { "perimeter_feed_rate" };
    def->min = 0;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloat(60));

    def = this->add("perimeters", coInt);
    def->label = L("Perimeters");
    def->full_label = ("Perimeters count");
    def->category = L("Layers and Perimeters");
    def->tooltip = L("This option sets the number of perimeters to generate for each layer. "
                   "Note that Slic3r may increase this number automatically when it detects "
                   "sloping surfaces which benefit from a higher number of perimeters "
                   "if the Extra Perimeters option is enabled.");
    def->sidetext = L("(minimum).");
    def->aliases = { "perimeter_offsets" };
    def->min = 0;
    def->max = 10000;
    def->set_default_value(new ConfigOptionInt(3));

    def = this->add("post_process", coStrings);
    def->label = L("Post-processing scripts");
    def->tooltip = L("If you want to process the output G-code through custom scripts, "
                   "just list their absolute paths here. Separate multiple scripts with a semicolon. "
                   "Scripts will be passed the absolute path to the G-code file as the first argument, "
                   "and they can access the Slic3r config settings by reading environment variables.");
    def->gui_flags = "serialized";
    def->multiline = true;
    def->full_width = true;
	def->height = 6;
    def->mode = comExpert;
	def->set_default_value(new ConfigOptionStrings());

    def = this->add("printer_model", coString);
    def->label = L("Printer type");
    def->tooltip = L("Type of the printer.");
    def->set_default_value(new ConfigOptionString());
    def->cli = ConfigOptionDef::nocli;

    def = this->add("printer_notes", coString);
    def->label = L("Printer notes");
    def->tooltip = L("You can put your notes regarding the printer here.");
    def->multiline = true;
    def->full_width = true;
    def->height = 13;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionString(""));

    def = this->add("printer_vendor", coString);
    def->label = L("Printer vendor");
    def->tooltip = L("Name of the printer vendor.");
    def->set_default_value(new ConfigOptionString());
    def->cli = ConfigOptionDef::nocli;

    def = this->add("printer_variant", coString);
    def->label = L("Printer variant");
    def->tooltip = L("Name of the printer variant. For example, the printer variants may be differentiated by a nozzle diameter.");
    def->set_default_value(new ConfigOptionString());
    def->cli = ConfigOptionDef::nocli;

    def = this->add("print_settings_id", coString);
    def->set_default_value(new ConfigOptionString(""));
    def->cli = ConfigOptionDef::nocli;
    
    def = this->add("printer_settings_id", coString);
    def->set_default_value(new ConfigOptionString(""));
    def->cli = ConfigOptionDef::nocli;

    def = this->add("support_material_solid_first_layer", coBool);
    def->label = L("Solid first layer");
    def->category = L("Support material");
    def->tooltip = L("Use a solid layer instead of a raft for the layer that touch the build plate.");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionBool(false));

    def = this->add("raft_layers", coInt);
    def->label = L("Raft layers");
    def->category = L("Support material");
    def->tooltip = L("The object will be raised by this number of layers, and support material "
        "will be generated under it.");
    def->sidetext = L("layers");
    def->min = 0;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionInt(0));

    def = this->add("resolution", coFloat);
    def->label = L("Resolution");
    def->tooltip = L("Minimum detail resolution, used to simplify the input file for speeding up "
        "the slicing job and reducing memory usage. High-resolution models often carry "
        "more detail than printers can render. Set to zero to disable any simplification "
        "and use full resolution from input. "
        "\nNote: slic3r simplify the geometry with a treshold of 0.0125mm and has an internal resolution of 0.0001mm.");
    def->sidetext = L("mm");
    def->min = 0;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloat(0));

    def = this->add("retract_before_travel", coFloats);
    def->label = L("Minimum travel after retraction");
    def->tooltip = L("Retraction is not triggered when travel moves are shorter than this length.");
    def->sidetext = L("mm");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloats { 2. });

    def = this->add("retract_before_wipe", coPercents);
    def->label = L("Retract amount before wipe");
    def->tooltip = L("With bowden extruders, it may be wise to do some amount of quick retract "
                   "before doing the wipe movement.");
    def->sidetext = L("%");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionPercents { 0. });
    
    def = this->add("retract_layer_change", coBools);
    def->label = L("Retract on layer change");
    def->tooltip = L("This flag enforces a retraction whenever a Z move is done.");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionBools { false });

    def = this->add("retract_length", coFloats);
    def->label = L("Length");
    def->full_label = ("Retract length");
    def->full_label = L("Retraction Length");
    def->tooltip = L("When retraction is triggered, filament is pulled back by the specified amount "
                   "(the length is measured on raw filament, before it enters the extruder).");
    def->sidetext = L("mm (zero to disable)");
    def->set_default_value(new ConfigOptionFloats { 2. });

    def = this->add("retract_length_toolchange", coFloats);
    def->label = L("Length");
    def->full_label = ("Toolchange retract length");
    def->full_label = L("Retraction Length (Toolchange)");
    def->tooltip = L("When retraction is triggered before changing tool, filament is pulled back "
                   "by the specified amount (the length is measured on raw filament, before it enters "
                   "the extruder).");
    def->sidetext = L("mm (zero to disable)");
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloats { 10. });

    def = this->add("retract_lift", coFloats);
    def->label = L("Lift Z");
    def->tooltip = L("If you set this to a positive value, Z is quickly raised every time a retraction "
                   "is triggered. When using multiple extruders, only the setting for the first extruder "
                   "will be considered.");
    def->sidetext = L("mm");
    def->set_default_value(new ConfigOptionFloats { 0. });

    def = this->add("retract_lift_above", coFloats);
    def->label = L("Above Z");
    def->full_label = ("Retract above Z");
    def->full_label = L("Only lift Z above");
    def->tooltip = L("If you set this to a positive value, Z lift will only take place above the specified "
                   "absolute Z. You can tune this setting for skipping lift on the first layers.");
    def->sidetext = L("mm");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloats { 0. });

    def = this->add("retract_lift_below", coFloats);
    def->label = L("Below Z");
    def->full_label = ("Retract below Z");
    def->full_label = L("Only lift Z below");
    def->tooltip = L("If you set this to a positive value, Z lift will only take place below "
                   "the specified absolute Z. You can tune this setting for limiting lift "
                   "to the first layers.");
    def->sidetext = L("mm");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloats { 0. });

    def = this->add("retract_lift_not_last_layer", coBools);
    def->label = L("Not on top");
    def->full_label = ("Don't retract on top surfaces");
    def->category = L("Support material");
    def->tooltip = L("Select this option to not use the z-lift on a top surface.");
    def->mode = comAdvanced;
	def->set_default_value(new ConfigOptionBools { false });

    def = this->add("retract_restart_extra", coFloats);
    def->label = L("Extra length on restart");
    def->tooltip = L("When the retraction is compensated after the travel move, the extruder will push "
                   "this additional amount of filament. This setting is rarely needed.");
    def->sidetext = L("mm");
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloats { 0. });

    def = this->add("retract_restart_extra_toolchange", coFloats);
    def->label = L("Extra length on restart");
    def->full_label = ("Extrat length on toolchange restart");
    def->tooltip = L("When the retraction is compensated after changing tool, the extruder will push "
                   "this additional amount of filament.");
    def->sidetext = L("mm");
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloats { 0. });

    def = this->add("retract_speed", coFloats);
    def->label = L("Retraction Speed");
    def->full_label = L("Retraction Speed");
    def->tooltip = L("The speed for retractions (it only applies to the extruder motor).");
    def->sidetext = L("mm/s");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloats { 40. });

    def = this->add("deretract_speed", coFloats);
    def->label = L("Deretraction Speed");
    def->full_label = L("Deretraction Speed");
    def->tooltip = L("The speed for loading of a filament into extruder after retraction "
                   "(it only applies to the extruder motor). If left to zero, the retraction speed is used.");
    def->sidetext = L("mm/s");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloats { 0. });

    def = this->add("seam_position", coEnum);
    def->label = L("Seam position");
    def->category = L("Layers and Perimeters");
    def->tooltip = L("Position of perimeters starting points.");
    def->enum_keys_map = &ConfigOptionEnum<SeamPosition>::get_enum_values();
    def->enum_values.push_back("random");
    def->enum_values.push_back("nearest");
    def->enum_values.push_back("aligned");
    def->enum_values.push_back("rear");
    def->enum_values.push_back("hidden");
    def->enum_labels.push_back(L("Random"));
    def->enum_labels.push_back(L("Nearest"));
    def->enum_labels.push_back(L("Aligned"));
    def->enum_labels.push_back(L("Rear"));
    def->enum_labels.push_back(L("Hidden"));
    def->mode = comSimple;
    def->set_default_value(new ConfigOptionEnum<SeamPosition>(spHidden));

    def = this->add("seam_travel", coBool);
    def->label = L("Travel move reduced");
    def->category = L("Layers and Perimeters");
    def->tooltip = L("Add a big cost to travel paths when possible (when going into a loop), so it will prefer a less optimal seam posistion if it's nearer.");
    def->cli = "seam-travel!";
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionBool(false));

#if 0
    def = this->add("seam_preferred_direction", coFloat);
//    def->gui_type = "slider";
    def->label = L("Direction");
    def->sidetext = L("°");
    def->full_label = L("Preferred direction of the seam");
    def->tooltip = L("Seam preferred direction");
    def->min = 0;
    def->max = 360;
    def->set_default_value(new ConfigOptionFloat(0);
    def->set_default_value(new ConfigOptionFloat(0));

    def = this->add("seam_preferred_direction_jitter", coFloat);
//    def->gui_type = "slider";
    def->label = L("Jitter");
    def->sidetext = L("°");
    def->full_label = L("Seam preferred direction jitter");
    def->tooltip = L("Preferred direction of the seam - jitter");
    def->min = 0;
    def->max = 360;
    def->set_default_value(new ConfigOptionFloat(30);
    def->set_default_value(new ConfigOptionFloat(30));
#endif

    def = this->add("serial_port", coString);
    def->gui_type = "select_open";
    def->label = "";
    def->full_label = L("Serial port");
    def->tooltip = L("USB/serial port for printer connection.");
    def->width = 20;
    def->set_default_value(new ConfigOptionString(""));

    def = this->add("serial_speed", coInt);
    def->gui_type = "i_enum_open";
    def->label = L("Speed");
    def->full_label = L("Serial port speed");
    def->tooltip = L("Speed (baud) of USB/serial port for printer connection.");
    def->min = 1;
    def->max = 300000;
    def->enum_values.push_back("115200");
    def->enum_values.push_back("250000");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionInt(250000));

    def = this->add("skirt_distance", coFloat);
    def->label = L("Distance from object");
    def->tooltip = L("Distance between skirt and object(s). Set this to zero to attach the skirt "
                   "to the object(s) and get a brim for better adhesion.");
    def->sidetext = L("mm");
    def->min = 0;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloat(6));

    def = this->add("skirt_height", coInt);
    def->label = L("Skirt height");
    def->tooltip = L("Height of skirt expressed in layers. Set this to a tall value to use skirt "
                   "as a shield against drafts.");
    def->sidetext = L("layers");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionInt(1));

    def = this->add("skirts", coInt);
    def->label = L("Loops (minimum)");
    def->full_label = L("Skirt Loops");
    def->tooltip = L("Number of loops for the skirt. If the Minimum Extrusion Length option is set, "
                   "the number of loops might be greater than the one configured here. Set this to zero "
                   "to disable skirt completely.");
    def->min = 0;
    def->mode = comSimple;
    def->set_default_value(new ConfigOptionInt(1));
    
    def = this->add("slowdown_below_layer_time", coInts);
    def->label = L("Slow down if layer print time is below");
    def->tooltip = L("If layer print time is estimated below this number of seconds, print moves "
                   "speed will be scaled down to extend duration to this value.");
    def->sidetext = L("approximate seconds");
    def->width = 6;
    def->min = 0;
    def->max = 1000;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionInts { 5 });

    def = this->add("small_perimeter_speed", coFloatOrPercent);
    def->label = L("Small");
    def->full_label = ("Small perimeters speed");
    def->category = L("Speed");
    def->tooltip = L("This separate setting will affect the speed of perimeters having radius <= 6.5mm "
                   "(usually holes). If expressed as percentage (for example: 80%) it will be calculated "
                   "on the perimeters speed setting above. Set to zero for auto.");
    def->sidetext = L("mm/s or %");
    def->ratio_over = "perimeter_speed";
    def->min = 0;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloatOrPercent(15, false));

    def = this->add("curve_smoothing_angle_convex", coFloat);
    def->label = L("Min convex angle");
    def->full_label = L("Curve smoothing minimum angle (convex)");
    def->category = L("Slicing");
    def->tooltip = L("Minimum (convex) angle at a vertex to enable smoothing"
        " (trying to create a curve around the vertex). "
        "180 : nothing will be smooth, 0 : all angles will be smoothen.");
    def->sidetext = L("°");
    def->aliases = { "curve_smoothing_angle" };
    def->cli = "curve-smoothing-angle-convex=f";
    def->min = 0;
    def->max = 180;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloat(0));

    def = this->add("curve_smoothing_angle_concave", coFloat);
    def->label = L("Min concave angle");
    def->full_label = L("Curve smoothing minimum angle (concave)");
    def->category = L("Slicing");
    def->tooltip = L("Minimum (concave) angle at a vertex to enable smoothing"
        " (trying to create a curve around the vertex). "
        "180 : nothing will be smooth, 0 : all angles will be smoothen.");
    def->sidetext = L("°");
    def->cli = "curve-smoothing-angle-concave=f";
    def->min = 0;
    def->max = 180;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloat(0));

    def = this->add("curve_smoothing_precision", coFloat);
    def->label = L("Precision");
    def->full_label = L("Curve smoothing precision");
    def->category = L("Slicing");
    def->tooltip = L("These parameter allow the slicer to smooth the angles in each layer. "
        "The precision will be at least the new precision of the curve. Set to 0 to deactivate."
        "\nNote: as it use the polygon's edges and only work in the 2D planes, "
        "you must have a very clean or hand-made 3D model."
        "\nIt's really only useful to smoothen functional models or very wide angles.");
    def->sidetext = L("mm");
    def->min = 0;
    def->cli = "curve-smoothing-precision=f";
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloat(0));

    def = this->add("curve_smoothing_cutoff_dist", coFloat);
    def->label = L("cutoff");
    def->full_label = L("Curve smoothing cutoff dist");
    def->category = L("Slicing");
    def->tooltip = L("Maximum distance between two points to allow adding new ones. Allow to avoid distording long strait areas. 0 to disable.");
    def->sidetext = L("mm");
    def->min = 0;
    def->cli = "curve-smoothing-cutoff-dist=f";
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloat(2));

    def = this->add("solid_infill_below_area", coFloat);
    def->label = L("Solid infill threshold area");
    def->category = L("Infill");
    def->tooltip = L("Force solid infill for regions having a smaller area than the specified threshold.");
    def->sidetext = L("mm²");
    def->min = 0;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloat(70));

    def = this->add("solid_infill_extruder", coInt);
    def->label = L("Solid infill extruder");
    def->category = L("Extruders");
    def->tooltip = L("The extruder to use when printing solid infill.");
    def->min = 1;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionInt(1));

    def = this->add("solid_infill_every_layers", coInt);
    def->label = L("Solid infill every");
    def->category = L("Infill");
    def->tooltip = L("This feature allows to force a solid layer every given number of layers. "
                   "Zero to disable. You can set this to any value (for example 9999); "
                   "Slic3r will automatically choose the maximum possible number of layers "
                   "to combine according to nozzle diameter and layer height.");
    def->sidetext = L("layers");
    def->min = 0;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionInt(0));

    def = this->add("solid_infill_extrusion_width", coFloatOrPercent);
    def->label = L("Solid infill");
    def->full_label = ("Solid infill width");
    def->category = L("Extrusion Width");
    def->tooltip = L("Set this to a non-zero value to set a manual extrusion width for infill for solid surfaces. "
                   "If left zero, default extrusion width will be used if set, otherwise 1.125 x nozzle diameter will be used. "
                   "If expressed as percentage (for example 110%) it will be computed over nozzle diameter.");
    def->sidetext = L("mm or %");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloatOrPercent(0, false));

    def = this->add("solid_infill_speed", coFloatOrPercent);
    def->label = L("Solid");
    def->full_label = ("Solid infill speed");
    def->category = L("Speed");
    def->tooltip = L("Speed for printing solid regions (top/bottom/internal horizontal shells). "
                   "This can be expressed as a percentage (for example: 80%) over the default "
                   "infill speed above. Set to zero for auto.");
    def->sidetext = L("mm/s or %");
    def->ratio_over = "infill_speed";
    def->aliases = { "solid_infill_feed_rate" };
    def->min = 0;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloatOrPercent(20, false));

    def = this->add("solid_layers", coInt);
    def->label = L("Solid layers");
    def->tooltip = L("Number of solid layers to generate on top and bottom surfaces.");
    def->shortcut.push_back("top_solid_layers");
    def->shortcut.push_back("bottom_solid_layers");
    def->min = 0;

    def = this->add("spiral_vase", coBool);
    def->label = L("Spiral vase");
    def->tooltip = L("This feature will raise Z gradually while printing a single-walled object "
                   "in order to remove any visible seam. This option requires a single perimeter, "
                   "no infill, no top solid layers and no support material. You can still set "
                   "any number of bottom solid layers as well as skirt/brim loops. "
                   "It won't work when printing more than an object.");
    def->set_default_value(new ConfigOptionBool(false));

    def = this->add("standby_temperature_delta", coInt);
    def->label = L("Temperature variation");
    def->tooltip = L("Temperature difference to be applied when an extruder is not active. "
                   "Enables a full-height \"sacrificial\" skirt on which the nozzles are periodically wiped.");
    def->sidetext = "∆°C";
    def->min = -max_temp;
    def->max = max_temp;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionInt(-5));

    def = this->add("start_gcode", coString);
    def->label = L("Start G-code");
    def->tooltip = L("This start procedure is inserted at the beginning, after bed has reached "
                   "the target temperature and extruder just started heating, and before extruder "
                   "has finished heating. If Slic3r detects M104 or M190 in your custom codes, "
                   "such commands will not be prepended automatically so you're free to customize "
                   "the order of heating commands and other custom actions. Note that you can use "
                   "placeholder variables for all Slic3r settings, so you can put "
                   "a \"M109 S[first_layer_temperature]\" command wherever you want.");
    def->multiline = true;
    def->full_width = true;
    def->height = 12;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionString("G28 ; home all axes\nG1 Z5 F5000 ; lift nozzle\n"));

    def = this->add("start_filament_gcode", coStrings);
    def->label = L("Start G-code");
    def->full_label = ("Filament start G-code");
    def->tooltip = L("This start procedure is inserted at the beginning, after any printer start gcode (and "
                   "after any toolchange to this filament in case of multi-material printers). "
                   "This is used to override settings for a specific filament. If Slic3r detects "
                   "M104, M109, M140 or M190 in your custom codes, such commands will "
                   "not be prepended automatically so you're free to customize the order "
                   "of heating commands and other custom actions. Note that you can use placeholder variables "
                   "for all Slic3r settings, so you can put a \"M109 S[first_layer_temperature]\" command "
                   "wherever you want. If you have multiple extruders, the gcode is processed "
                   "in extruder order.");
    def->multiline = true;
    def->full_width = true;
    def->height = 12;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionStrings { "; Filament gcode\n" });


    def = this->add("model_precision", coFloat);
    def->label = L("Model rounding precision");
    def->full_label = L("Model rounding precision");
    def->category = L("Slicing");
    def->tooltip = L("This is the rounding error of the input object."
        " It's used to align points that should be in the same line."
        " Put 0 to disable.");
    def->sidetext = L("mm");
    def->min = 0;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloat(0.0001));

    def = this->add("single_extruder_multi_material", coBool);
    def->label = L("Single Extruder Multi Material");
    def->tooltip = L("The printer multiplexes filaments into a single hot end.");
    def->mode = comAdvanced;
	def->set_default_value(new ConfigOptionBool(false));

    def = this->add("single_extruder_multi_material_priming", coBool);
    def->label = L("Prime all printing extruders");
    def->tooltip = L("If enabled, all printing extruders will be primed at the front edge of the print bed at the start of the print.");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionBool(true));

    def = this->add("support_material", coBool);
    def->label = L("Generate support material");
    def->category = L("Support material");
    def->tooltip = L("Enable support material generation.");
    def->set_default_value(new ConfigOptionBool(false));

    def = this->add("support_material_auto", coBool);
    def->label = L("Auto generated supports");
    def->category = L("Support material");
    def->tooltip = L("If checked, supports will be generated automatically based on the overhang threshold value."\
                     " If unchecked, supports will be generated inside the \"Support Enforcer\" volumes only.");
    def->mode = comSimple;
    def->set_default_value(new ConfigOptionBool(true));

    def = this->add("support_material_xy_spacing", coFloatOrPercent);
    def->label = L("XY separation between an object and its support");
    def->category = L("Support material");
    def->tooltip = L("XY separation between an object and its support. If expressed as percentage "
                   "(for example 50%), it will be calculated over external perimeter width.");
    def->sidetext = L("mm or %");
    def->ratio_over = "external_perimeter_extrusion_width";
    def->min = 0;
    def->mode = comAdvanced;
    // Default is half the external perimeter width.
    def->set_default_value(new ConfigOptionFloatOrPercent(50, true));

    def = this->add("support_material_angle", coFloat);
    def->label = L("Pattern angle");
    def->full_label = ("Support pattern angle");
    def->category = L("Support material");
    def->tooltip = L("Use this setting to rotate the support material pattern on the horizontal plane.");
    def->sidetext = L("°");
    def->min = 0;
    def->max = 359;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloat(0));

    def = this->add("support_material_buildplate_only", coBool);
    def->label = L("Support on build plate only");
    def->category = L("Support material");
    def->tooltip = L("Only create support if it lies on a build plate. Don't create support on a print.");
    def->mode = comSimple;
    def->set_default_value(new ConfigOptionBool(false));

    def = this->add("support_material_contact_distance_type", coEnum);
    def->label = L("Type");
    def->full_label = ("Support contact distance type");
    def->category = L("Support material");
    def->tooltip = L("How to compute the vertical z-distance.\n"
        "From filament: it use the nearest bit of the filament. When a bridge is extruded, it goes below the current plane.\n"
        "From plane: it use the plane-z. Same than 'from filament' if no 'bridge' is extruded.\n"
        "None: No z-offset. Useful for Soluble supports.\n");
    def->enum_keys_map = &ConfigOptionEnum<SupportZDistanceType>::get_enum_values();
    def->enum_values.push_back("filament");
    def->enum_values.push_back("plane");
    def->enum_values.push_back("none");
    def->enum_labels.push_back("From filament");
    def->enum_labels.push_back("From plane");
    def->enum_labels.push_back("None");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionEnum<SupportZDistanceType>(zdPlane));

    def = this->add("support_material_contact_distance_top", coFloatOrPercent);
    def->label = L("Top");
    def->full_label = ("Contact distance on top of supports");
    def->category = L("Support material");
    def->tooltip = L("The vertical distance between support material interface and the object"
        "(when the object is printed on top of the support). "
        "Setting this to 0 will also prevent Slic3r from using bridge flow and speed "
        "for the first object layer. Can be a % of the extruding width used for the interface layers.");
    def->sidetext = L("mm");
    def->enum_labels.push_back((boost::format("0.2 (%1%)") % L("detachable")).str());
//    def->min = 0;
    def->enum_values.push_back("0");
    def->enum_values.push_back("0.2");
	def->enum_labels.push_back(L("0 (soluble)"));
	def->enum_labels.push_back(L("0.2 (detachable)"));
    def->mode = comAdvanced;
    def->aliases = { "support_material_contact_distance" };
    def->set_default_value(new ConfigOptionFloatOrPercent(0.2, false));

    def = this->add("support_material_contact_distance_bottom", coFloatOrPercent);
    def->label = L("Bottom");
    def->full_label = ("Contact distance under the bottom of supports");
    def->category = L("Support material");
    def->tooltip = L("The vertical distance between object and support material interface"
        "(when the support is printed on top of the object). Can be a % of the extruding width used for the interface layers.");
    def->sidetext = L("mm");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloatOrPercent(0.2,false));

    def = this->add("support_material_enforce_layers", coInt);
    def->label = L("Enforce support for the first");
    def->category = L("Support material");
    def->tooltip = L("Generate support material for the specified number of layers counting from bottom, "
                   "regardless of whether normal support material is enabled or not and regardless "
                   "of any angle threshold. This is useful for getting more adhesion of objects "
                   "having a very thin or poor footprint on the build plate.");
    def->sidetext = L("layers");
    def->full_label = L("Enforce support for the first n layers");
    def->min = 0;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionInt(0));

    def = this->add("support_material_extruder", coInt);
    def->label = L("Support material/raft/skirt extruder");
    def->category = L("Extruders");
    def->tooltip = L("The extruder to use when printing support material, raft and skirt "
                   "(1+, 0 to use the current extruder to minimize tool changes).");
    def->min = 0;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionInt(1));

    def = this->add("support_material_extrusion_width", coFloatOrPercent);
    def->label = L("Support material");
    def->full_label = L("Support material width");
    def->category = L("Extrusion Width");
    def->tooltip = L("Set this to a non-zero value to set a manual extrusion width for support material. "
                   "If left zero, default extrusion width will be used if set, otherwise nozzle diameter will be used. "
                   "If expressed as percentage (for example 110%) it will be computed over nozzle diameter.");
    def->sidetext = L("mm or %");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloatOrPercent(0, false));

    def = this->add("support_material_interface_contact_loops", coBool);
    def->label = L("Interface loops");
    def->category = L("Support material");
    def->tooltip = L("Cover the top contact layer of the supports with loops. Disabled by default.");
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionBool(false));

    def = this->add("support_material_interface_extruder", coInt);
    def->label = L("Support material/raft interface extruder");
    def->category = L("Extruders");
    def->tooltip = L("The extruder to use when printing support material interface "
                   "(1+, 0 to use the current extruder to minimize tool changes). This affects raft too.");
    def->min = 0;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionInt(1));

    def = this->add("support_material_interface_layers", coInt);
    def->label = L("Interface layers");
    def->category = L("Support material");
    def->tooltip = L("Number of interface layers to insert between the object(s) and support material.");
    def->sidetext = L("layers");
    def->min = 0;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionInt(3));

    def = this->add("support_material_interface_spacing", coFloat);
    def->label = L("Interface pattern spacing");
    def->category = L("Support material");
    def->tooltip = L("Spacing between interface lines. Set zero to get a solid interface.");
    def->sidetext = L("mm");
    def->min = 0;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloat(0));

    def = this->add("support_material_interface_speed", coFloatOrPercent);
    def->label = L("Interface");
    def->full_label = L("Support interface speed");
    def->category = L("Support material");
    def->tooltip = L("Speed for printing support material interface layers. If expressed as percentage "
                   "(for example 50%) it will be calculated over support material speed.");
    def->sidetext = L("mm/s or %");
    def->ratio_over = "support_material_speed";
    def->min = 0;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloatOrPercent(100, true));

    def = this->add("support_material_pattern", coEnum);
    def->label = L("Pattern");
    def->full_label = L("Support pattern");
    def->category = L("Support material");
    def->tooltip = L("Pattern used to generate support material.");
    def->enum_keys_map = &ConfigOptionEnum<SupportMaterialPattern>::get_enum_values();
    def->enum_values.push_back("rectilinear");
    def->enum_values.push_back("rectilinear-grid");
    def->enum_values.push_back("honeycomb");
    def->enum_labels.push_back(L("Rectilinear"));
    def->enum_labels.push_back(L("Rectilinear grid"));
    def->enum_labels.push_back(L("Honeycomb"));
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionEnum<SupportMaterialPattern>(smpRectilinear));

    def = this->add("support_material_interface_pattern", coEnum);
    def->label = L("Pattern");
    def->full_label = L("Support interface pattern");
    def->category = L("Support material");
    def->tooltip = L("Pattern for interface layer.");
    def->enum_keys_map = &ConfigOptionEnum<InfillPattern>::get_enum_values();
    def->enum_values.push_back("rectilinear");
    def->enum_values.push_back("concentric");
    def->enum_values.push_back("concentricgapfill");
    def->enum_values.push_back("hilbertcurve");
    def->enum_values.push_back("sawtooth");
    def->enum_values.push_back("smooth");
    def->enum_labels.push_back(L("Rectilinear"));
    def->enum_labels.push_back(L("Concentric"));
    def->enum_labels.push_back(L("Concentric (filled)"));
    def->enum_labels.push_back(L("Hilbert Curve"));
    def->enum_labels.push_back(L("Sawtooth"));
    def->enum_labels.push_back(L("Ironing"));
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionEnum<InfillPattern>(ipRectilinear));
    
    def = this->add("support_material_spacing", coFloat);
    def->label = L("Pattern spacing");
    def->category = L("Support material");
    def->tooltip = L("Spacing between support material lines.");
    def->sidetext = L("mm");
    def->min = 0;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloat(2.5));

    def = this->add("support_material_speed", coFloat);
    def->label = L("Default");
    def->full_label = L("Support speed");
    def->category = L("Support material");
    def->tooltip = L("Speed for printing support material.");
    def->sidetext = L("mm/s");
    def->min = 0;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloat(60));

    def = this->add("support_material_synchronize_layers", coBool);
    def->label = L("Synchronize with object layers");
    def->category = L("Support material");
    def->tooltip = L("Synchronize support layers with the object print layers. This is useful "
                   "with multi-material printers, where the extruder switch is expensive.");
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionBool(false));

    def = this->add("support_material_threshold", coInt);
    def->label = L("Overhang threshold");
    def->category = L("Support material");
    def->tooltip = L("Support material will not be generated for overhangs whose slope angle "
                   "(90° = vertical) is above the given threshold. In other words, this value "
                   "represent the most horizontal slope (measured from the horizontal plane) "
                   "that you can print without support material. Set to zero for automatic detection "
                   "(recommended).");
    def->sidetext = L("°");
    def->min = 0;
    def->max = 90;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionInt(0));

    def = this->add("support_material_with_sheath", coBool);
    def->label = L("With sheath around the support");
    def->category = L("Support material");
    def->tooltip = L("Add a sheath (a single perimeter line) around the base support. This makes "
                   "the support more reliable, but also more difficult to remove.");
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionBool(true));

    def = this->add("temperature", coInts);
    def->label = L("Other layers");
    def->full_label = L("Temperature");
    def->tooltip = L("Extruder temperature for layers after the first one. Set this to zero to disable "
                   "temperature control commands in the output.");
    def->full_label = L("Temperature");
    def->sidetext = L("°C");
    def->min = 0;
    def->max = max_temp;
    def->set_default_value(new ConfigOptionInts { 200 });
    
    def = this->add("thin_walls", coBool);
    def->label = L("");
    def->full_label = L("Thin walls");
    def->category = L("Layers and Perimeters");
    def->tooltip = L("Detect single-width walls (parts where two extrusions don't fit and we need "
                   "to collapse them into a single trace). If unchecked, slic3r may try to fit perimeters "
                   "where it's not possible, creating some overlap leading to over-extrusion.");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionBool(true));

    def = this->add("thin_walls_min_width", coFloatOrPercent);
    def->label = L("min width");
    def->full_label = L("Thin walls min width");
    def->category = L("Layers and Perimeters");
    def->tooltip = L("Minimum width for the extrusion to be extruded (widths lower than the nozzle diameter will be over-extruded at the nozzle diameter)."
        " If expressed as percentage (for example 110%) it will be computed over nozzle diameter."
        " The default behavior of slic3r and slic3rPE is with a 33% value. Put 100% to avoid any sort of over-extrusion.");
    def->mode = comExpert;
    def->min = 0;
    def->set_default_value(new ConfigOptionFloatOrPercent(33, true));

    def = this->add("thin_walls_overlap", coFloatOrPercent);
    def->label = L("overlap");
    def->full_label = L("Thin wall overlap");
    def->category = L("Layers and Perimeters");
    def->tooltip = L("Overlap between the thin wall and the perimeters. Can be a % of the external perimeter width (default 50%)");
    def->mode = comExpert;
    def->min = 0;
    def->set_default_value(new ConfigOptionFloatOrPercent(50, true));

    def = this->add("threads", coInt);
    def->label = L("Threads");
    def->tooltip = L("Threads are used to parallelize long-running tasks. Optimal threads number "
                   "is slightly above the number of available cores/processors.");
    def->readonly = true;
    def->min = 1;
    {
        int threads = (unsigned int)boost::thread::hardware_concurrency();
        def->set_default_value(new ConfigOptionInt(threads > 0 ? threads : 2));
        def->cli == ConfigOptionDef::nocli;
    }
    
    def = this->add("toolchange_gcode", coString);
    def->label = L("Tool change G-code");
    def->tooltip = L("This custom code is inserted at every extruder change. If you don't leave this empty, you are "
                     "expected to take care of the toolchange yourself - PrusaSlicer will not output any other G-code to "
                     "change the filament. You can use placeholder variables for all Slic3r settings as well as [previous_extruder] "
                     "and [next_extruder], so e.g. the standard toolchange command can be scripted as T[next_extruder].");
    def->multiline = true;
    def->full_width = true;
    def->height = 5;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionString(""));

    def = this->add("top_infill_extrusion_width", coFloatOrPercent);
    def->label = L("Top solid infill");
    def->category = L("Extrusion Width");
    def->tooltip = L("Set this to a non-zero value to set a manual extrusion width for infill for top surfaces. "
                   "You may want to use thinner extrudates to fill all narrow regions and get a smoother finish. "
                   "If left zero, default extrusion width will be used if set, otherwise nozzle diameter will be used. "
                   "If expressed as percentage (for example 110%) it will be computed over nozzle diameter.");
    def->sidetext = L("mm or %");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloatOrPercent(0, false));

    def = this->add("top_solid_infill_speed", coFloatOrPercent);
    def->label = L("Top solid");
    def->full_label = L("Top solid speed");
    def->category = L("Speed");
    def->tooltip = L("Speed for printing top solid layers (it only applies to the uppermost "
                   "external layers and not to their internal solid layers). You may want "
                   "to slow down this to get a nicer surface finish. This can be expressed "
                   "as a percentage (for example: 80%) over the solid infill speed above. "
                   "Set to zero for auto.");
    def->sidetext = L("mm/s or %");
    def->ratio_over = "solid_infill_speed";
    def->min = 0;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloatOrPercent(15, false));

    def = this->add("top_solid_layers", coInt);
    //TRN To be shown in Print Settings "Top solid layers"
    def->label = L("Top");
    def->full_label = L("Top layers");
    def->category = L("Layers and Perimeters");
    def->tooltip = L("Number of solid layers to generate on top surfaces.");
    def->full_label = L("Top solid layers");
    def->min = 0;
    def->set_default_value(new ConfigOptionInt(3));

    def = this->add("travel_speed", coFloat);
    def->label = L("Travel");
    def->full_label = L("Travel speed");
    def->tooltip = L("Speed for travel moves (jumps between distant extrusion points).");
    def->sidetext = L("mm/s");
    def->aliases = { "travel_feed_rate" };
    def->min = 1;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloat(130));

    def = this->add("use_firmware_retraction", coBool);
    def->label = L("Use firmware retraction");
    def->tooltip = L("This experimental setting uses G10 and G11 commands to have the firmware "
                   "handle the retraction. This is only supported in recent Marlin.");
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionBool(false));

    def = this->add("use_relative_e_distances", coBool);
    def->label = L("Use relative E distances");
    def->tooltip = L("If your firmware requires relative E values, check this, "
                   "otherwise leave it unchecked. Most firmwares use absolute values.");
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionBool(false));

    def = this->add("use_volumetric_e", coBool);
    def->label = L("Use volumetric E");
    def->tooltip = L("This experimental setting uses outputs the E values in cubic millimeters "
                   "instead of linear millimeters. If your firmware doesn't already know "
                   "filament diameter(s), you can put commands like 'M200 D[filament_diameter_0] T0' "
                   "in your start G-code in order to turn volumetric mode on and use the filament "
                   "diameter associated to the filament selected in Slic3r. This is only supported "
                   "in recent Marlin.");
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionBool(false));

    def = this->add("variable_layer_height", coBool);
    def->label = L("Enable variable layer height feature");
    def->tooltip = L("Some printers or printer setups may have difficulties printing "
                   "with a variable layer height. Enabled by default.");
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionBool(true));

    def = this->add("wipe", coBools);
    def->label = L("Wipe while retracting");
    def->tooltip = L("This flag will move the nozzle while retracting to minimize the possible blob "
                   "on leaky extruders.");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionBools { false });

    def = this->add("wipe_tower", coBool);
    def->label = L("Enable");
    def->full_label = L("Enable wipe tower");
    def->tooltip = L("Multi material printers may need to prime or purge extruders on tool changes. "
                   "Extrude the excess material into the wipe tower.");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionBool(false));

    def = this->add("wiping_volumes_extruders", coFloats);
    def->label = L("Purging volumes - load/unload volumes");
    def->tooltip = L("This vector saves required volumes to change from/to each tool used on the "
                     "wipe tower. These values are used to simplify creation of the full purging "
                     "volumes below. ");
    def->set_default_value(new ConfigOptionFloats { 70.f, 70.f, 70.f, 70.f, 70.f, 70.f, 70.f, 70.f, 70.f, 70.f  });

    def = this->add("wiping_volumes_matrix", coFloats);
    def->label = L("Purging volumes - matrix");
    def->tooltip = L("This matrix describes volumes (in cubic milimetres) required to purge the"
                     " new filament on the wipe tower for any given pair of tools. ");
    def->set_default_value(new ConfigOptionFloats {   0.f, 140.f, 140.f, 140.f, 140.f,
                                                  140.f,   0.f, 140.f, 140.f, 140.f,
                                                  140.f, 140.f,   0.f, 140.f, 140.f,
                                                  140.f, 140.f, 140.f,   0.f, 140.f,
                                                    140.f, 140.f, 140.f, 140.f,   0.f });


    def = this->add("wipe_advanced", coBool);
    def->label = L("Enable advanced wiping volume");
    def->tooltip = L("Allow slic3r to compute the purge volume via smart computations. Use the pigment% of each filament and following parameters");
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionBool(false));

    def = this->add("wipe_advanced_nozzle_melted_volume", coFloat);
    def->label = L("Nozzle volume");
    def->tooltip = L("The volume of melted plastic inside your nozlle. Used by 'advanced wiping'.");
    def->sidetext = L("mm3");
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloat(120));

    def = this->add("filament_wipe_advanced_pigment", coFloats);
    def->label = L("Pigment percentage");
    def->tooltip = L("The pigment % for this filament (bewteen 0 and 1, 1=100%). 0 for translucent/natural, 0.2-0.5 for white and 1 for black.");
    def->mode = comExpert;
    def->min = 0;
    def->max = 1;
    def->set_default_value(new ConfigOptionFloats{ 0.5 });

    def = this->add("wipe_advanced_multiplier", coFloat);
    def->label = L("Multiplier");
    def->full_label = L("Auto-wipe multiplier");
    def->tooltip = L("The volume multiplier used to compute the final volume to extrude by the algorithm.");
    def->sidetext = L("mm3");
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloat(60));


    def = this->add("wipe_advanced_algo", coEnum);
    def->label = L("Algorithm");
    def->full_label = L("Auto-wipe algorithm");
    def->tooltip = L("Algo for the advanced wipe.\n"
        "Linear : volume = nozzle + volume_mult * (pigmentBefore-pigmentAfter)\n"
        "Quadratic: volume = nozzle + volume_mult * (pigmentBefore-pigmentAfter)+ volume_mult * (pigmentBefore-pigmentAfter)^3\n"
        "Hyperbola: volume = nozzle + volume_mult * (0.5+pigmentBefore) / (0.5+pigmentAfter)");
    def->enum_keys_map = &ConfigOptionEnum<WipeAlgo>::get_enum_values();
    def->enum_values.push_back("linear");
    def->enum_values.push_back("quadra");
    def->enum_values.push_back("expo");
    def->enum_labels.push_back(L("Linear"));
    def->enum_labels.push_back(L("Quadratric"));
    def->enum_labels.push_back(L("Hyperbola"));
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionEnum<WipeAlgo>(waLinear));

    def = this->add("wipe_tower_x", coFloat);
    def->label = L("X");
    def->full_label = L("Wipe tower X");
    def->tooltip = L("X coordinate of the left front corner of a wipe tower");
    def->sidetext = L("mm");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloat(180.));

    def = this->add("wipe_tower_y", coFloat);
    def->label = L("Y");
    def->full_label = L("Wipe tower Y");
    def->tooltip = L("Y coordinate of the left front corner of a wipe tower");
    def->sidetext = L("mm");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloat(140.));

    def = this->add("wipe_tower_width", coFloat);
    def->label = L("Width");
    def->full_label = L("Wipe tower Width");
    def->tooltip = L("Width of a wipe tower");
    def->sidetext = L("mm");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloat(60.));

    def = this->add("wipe_tower_rotation_angle", coFloat);
    def->label = L("Wipe tower rotation angle");
    def->tooltip = L("Wipe tower rotation angle with respect to x-axis.");
    def->sidetext = L("°");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloat(0.));

    def = this->add("wipe_into_infill", coBool);
    def->category = L("Wipe options");
    def->label = L("Wipe into this object's infill");
    def->tooltip = L("Purging after toolchange will done inside this object's infills. "
                     "This lowers the amount of waste but may result in longer print time "
                     " due to additional travel moves.");
    def->set_default_value(new ConfigOptionBool(false));

    def = this->add("wipe_into_objects", coBool);
    def->category = L("Wipe options");
    def->label = L("Wipe into this object");
    def->tooltip = L("Object will be used to purge the nozzle after a toolchange to save material "
                     "that would otherwise end up in the wipe tower and decrease print time. "
                     "Colours of the objects will be mixed as a result.");
    def->set_default_value(new ConfigOptionBool(false));

    def = this->add("wipe_tower_bridging", coFloat);
    def->label = L("Maximal bridging distance");
    def->tooltip = L("Maximal distance between supports on sparse infill sections. ");
    def->sidetext = L("mm");
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloat(10.));

    def = this->add("xy_size_compensation", coFloat);
    def->label = L("All layers");
    def->full_label = L("XY size compensation");
    def->category = L("Slicing");
    def->tooltip = L("The object will be grown/shrunk in the XY plane by the configured value "
                   "(negative = inwards, positive = outwards). This might be useful "
                   "for fine-tuning sizes.");
    def->sidetext = L("mm");
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloat(0));

    def = this->add("hole_size_compensation", coFloat);
    def->label = L("Holes");
    def->full_label = L("XY holes compensation");
    def->category = L("Slicing");
    def->tooltip = L("The convex holes will be grown / shrunk in the XY plane by the configured value"
                   " (negative = inwards, positive = outwards, should be negative as the holes are always a bit smaller irl)."
                   " This might be useful for fine-tuning hole sizes.");
    def->sidetext = L("mm");
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloat(0));

    def = this->add("z_offset", coFloat);
    def->label = L("Z offset");
    def->tooltip = L("This value will be added (or subtracted) from all the Z coordinates "
                   "in the output G-code. It is used to compensate for bad Z endstop position: "
                   "for example, if your endstop zero actually leaves the nozzle 0.3mm far "
                   "from the print bed, set this to -0.3 (or fix your endstop).");
    def->sidetext = L("mm");
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloat(0));

    // Declare retract values for filament profile, overriding the printer's extruder profile.
    for (const char *opt_key : {
    	// floats
		"retract_length", "retract_lift", "retract_lift_above", "retract_lift_below", "retract_speed", "deretract_speed", "retract_restart_extra", "retract_before_travel", 
		// bools
		"retract_layer_change", "wipe",
		// percents
		"retract_before_wipe"}) {
    	auto it_opt = options.find(opt_key);
    	assert(it_opt != options.end());
    	def = this->add_nullable(std::string("filament_") + opt_key, it_opt->second.type);
    	def->label 		= it_opt->second.label;
    	def->full_label = it_opt->second.full_label;
    	def->tooltip 	= it_opt->second.tooltip;
    	def->sidetext   = it_opt->second.sidetext;
    	def->mode       = it_opt->second.mode;
    	switch (def->type) {
    	case coFloats   : def->set_default_value(new ConfigOptionFloatsNullable  (static_cast<const ConfigOptionFloats*  >(it_opt->second.default_value.get())->values)); break;
    	case coPercents : def->set_default_value(new ConfigOptionPercentsNullable(static_cast<const ConfigOptionPercents*>(it_opt->second.default_value.get())->values)); break;
    	case coBools    : def->set_default_value(new ConfigOptionBoolsNullable   (static_cast<const ConfigOptionBools*   >(it_opt->second.default_value.get())->values)); break;
    	default: assert(false);
    	}
    }
}

void PrintConfigDef::init_extruder_retract_keys()
{
	m_extruder_retract_keys = {
		"deretract_speed",
		"retract_before_travel",
		"retract_before_wipe",
		"retract_layer_change",
		"retract_length",
		"retract_lift",
		"retract_lift_above",
		"retract_lift_below",
		"retract_restart_extra",
		"retract_speed",
		"wipe"
	};
	assert(std::is_sorted(m_extruder_retract_keys.begin(), m_extruder_retract_keys.end()));
}

void PrintConfigDef::init_sla_params()
{
    ConfigOptionDef* def;

    // SLA Printer settings

    def = this->add("display_width", coFloat);
    def->label = L("Display width");
    def->tooltip = L("Width of the display");
    def->min = 1;
    def->set_default_value(new ConfigOptionFloat(120.));

    def = this->add("display_height", coFloat);
    def->label = L("Display height");
    def->tooltip = L("Height of the display");
    def->min = 1;
    def->set_default_value(new ConfigOptionFloat(68.));

    def = this->add("display_pixels_x", coInt);
    def->full_label = L("Number of pixels in");
    def->label = ("X");
    def->tooltip = L("Number of pixels in X");
    def->min = 100;
    def->set_default_value(new ConfigOptionInt(2560));

    def = this->add("display_pixels_y", coInt);
    def->label = ("Y");
    def->tooltip = L("Number of pixels in Y");
    def->min = 100;
    def->set_default_value(new ConfigOptionInt(1440));

    def = this->add("display_mirror_x", coBool);
    def->full_label = L("Display horizontal mirroring");
    def->label = L("Mirror horizontally");
    def->tooltip = L("Enable horizontal mirroring of output images");
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionBool(true));

    def = this->add("display_mirror_y", coBool);
    def->full_label = L("Display vertical mirroring");
    def->label = L("Mirror vertically");
    def->tooltip = L("Enable vertical mirroring of output images");
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionBool(false));

    def = this->add("display_orientation", coEnum);
    def->label = L("Display orientation");
    def->tooltip = L("Set the actual LCD display orientation inside the SLA printer."
                     " Portrait mode will flip the meaning of display width and height parameters"
                     " and the output images will be rotated by 90 degrees.");
    def->enum_keys_map = &ConfigOptionEnum<SLADisplayOrientation>::get_enum_values();
    def->enum_values.push_back("landscape");
    def->enum_values.push_back("portrait");
    def->enum_labels.push_back(L("Landscape"));
    def->enum_labels.push_back(L("Portrait"));
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionEnum<SLADisplayOrientation>(sladoPortrait));

    def = this->add("fast_tilt_time", coFloat);
    def->label = L("Fast");
    def->full_label = L("Fast tilt");
    def->tooltip = L("Time of the fast tilt");
    def->sidetext = L("s");
    def->min = 0;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloat(5.));

    def = this->add("slow_tilt_time", coFloat);
    def->label = L("Slow");
    def->full_label = L("Slow tilt");
    def->tooltip = L("Time of the slow tilt");
    def->sidetext = L("s");
    def->min = 0;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloat(8.));

    def = this->add("area_fill", coFloat);
    def->label = L("Area fill");
    def->tooltip = L("The percentage of the bed area. \nIf the print area exceeds the specified value, \nthen a slow tilt will be used, otherwise - a fast tilt");
    def->sidetext = L("%");
    def->min = 0;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloat(50.));

    def = this->add("relative_correction", coFloats);
    def->label = L("Printer scaling correction");
    def->full_label = L("Printer scaling correction");
    def->tooltip  = L("Printer scaling correction");
    def->min = 0;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloats( { 1., 1. } ));

    def = this->add("absolute_correction", coFloat);
    def->label = L("Printer absolute correction");
    def->full_label = L("Printer absolute correction");
    def->tooltip  = L("Will inflate or deflate the sliced 2D polygons according "
                      "to the sign of the correction.");
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloat(0.0));
    
    def = this->add("gamma_correction", coFloat);
    def->label = L("Printer gamma correction");
    def->full_label = L("Printer gamma correction");
    def->tooltip  = L("This will apply a gamma correction to the rasterized 2D "
                      "polygons. A gamma value of zero means thresholding with "
                      "the threshold in the middle. This behaviour eliminates "
                      "antialiasing without losing holes in polygons.");
    def->min = 0;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloat(1.0));
    

    // SLA Material settings.
    def = this->add("initial_layer_height", coFloat);
    def->label = L("Initial layer height");
    def->tooltip = L("Initial layer height");
    def->sidetext = L("mm");
    def->min = 0;
    def->set_default_value(new ConfigOptionFloat(0.3));

    def = this->add("faded_layers", coInt);
    def->label = L("Faded layers");
    def->tooltip = L("Number of the layers needed for the exposure time fade from initial exposure time to the exposure time");
    def->min = 3;
    def->max = 20;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionInt(10));

    def = this->add("exposure_time", coFloat);
    def->label = L("Exposure time");
    def->tooltip = L("Exposure time");
    def->sidetext = L("s");
    def->min = 0;
    def->set_default_value(new ConfigOptionFloat(10));

    def = this->add("initial_exposure_time", coFloat);
    def->label = L("Initial exposure time");
    def->tooltip = L("Initial exposure time");
    def->sidetext = L("s");
    def->min = 0;
    def->set_default_value(new ConfigOptionFloat(15));

    def = this->add("material_correction", coFloats);
    def->full_label = L("Correction for expansion");
    def->tooltip  = L("Correction for expansion");
    def->min = 0;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloats( { 1. , 1. } ));

    def = this->add("material_notes", coString);
    def->label = L("SLA print material notes");
    def->tooltip = L("You can put your notes regarding the SLA print material here.");
    def->multiline = true;
    def->full_width = true;
    def->height = 13;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionString(""));

    def = this->add("default_sla_material_profile", coString);
    def->label = L("Default SLA material profile");
    def->tooltip = L("Default print profile associated with the current printer profile. "
                   "On selection of the current printer profile, this print profile will be activated.");
    def->set_default_value(new ConfigOptionString());
    def->cli = ConfigOptionDef::nocli;

    def = this->add("sla_material_settings_id", coString);
    def->set_default_value(new ConfigOptionString(""));
    def->cli = ConfigOptionDef::nocli;

    def = this->add("default_sla_print_profile", coString);
    def->label = L("Default SLA material profile");
    def->tooltip = L("Default print profile associated with the current printer profile. "
                   "On selection of the current printer profile, this print profile will be activated.");
    def->set_default_value(new ConfigOptionString());
    def->cli = ConfigOptionDef::nocli;

    def = this->add("sla_print_settings_id", coString);
    def->set_default_value(new ConfigOptionString(""));
    def->cli = ConfigOptionDef::nocli;

    def = this->add("supports_enable", coBool);
    def->label = L("Generate supports");
    def->category = L("Supports");
    def->tooltip = L("Generate supports for the models");
    def->mode = comSimple;
    def->set_default_value(new ConfigOptionBool(true));

    def = this->add("support_head_front_diameter", coFloat);
    def->label = L("Support head front diameter");
    def->category = L("Supports");
    def->tooltip = L("Diameter of the pointing side of the head");
    def->sidetext = L("mm");
    def->min = 0;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloat(0.4));

    def = this->add("support_head_penetration", coFloat);
    def->label = L("Support head penetration");
    def->category = L("Supports");
    def->tooltip = L("How much the pinhead has to penetrate the model surface");
    def->sidetext = L("mm");
    def->mode = comAdvanced;
    def->min = 0;
    def->set_default_value(new ConfigOptionFloat(0.2));

    def = this->add("support_head_width", coFloat);
    def->label = L("Support head width");
    def->category = L("Supports");
    def->tooltip = L("Width from the back sphere center to the front sphere center");
    def->sidetext = L("mm");
    def->min = 0;
    def->max = 20;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloat(1.0));

    def = this->add("support_pillar_diameter", coFloat);
    def->label = L("Support pillar diameter");
    def->category = L("Supports");
    def->tooltip = L("Diameter in mm of the support pillars");
    def->sidetext = L("mm");
    def->min = 0;
    def->max = 15;
    def->mode = comSimple;
    def->set_default_value(new ConfigOptionFloat(1.0));

    def = this->add("support_pillar_connection_mode", coEnum);
    def->label = L("Support pillar connection mode");
    def->tooltip = L("Controls the bridge type between two neighboring pillars."
                     " Can be zig-zag, cross (double zig-zag) or dynamic which"
                     " will automatically switch between the first two depending"
                     " on the distance of the two pillars.");
    def->enum_keys_map = &ConfigOptionEnum<SLAPillarConnectionMode>::get_enum_values();
    def->enum_values.push_back("zigzag");
    def->enum_values.push_back("cross");
    def->enum_values.push_back("dynamic");
    def->enum_labels.push_back(L("Zig-Zag"));
    def->enum_labels.push_back(L("Cross"));
    def->enum_labels.push_back(L("Dynamic"));
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionEnum<SLAPillarConnectionMode>(slapcmDynamic));

    def = this->add("support_buildplate_only", coBool);
    def->label = L("Support on build plate only");
    def->category = L("Supports");
    def->tooltip = L("Only create support if it lies on a build plate. Don't create support on a print.");
    def->mode = comSimple;
    def->set_default_value(new ConfigOptionBool(false));

    def = this->add("support_pillar_widening_factor", coFloat);
    def->label = L("Pillar widening factor");
    def->category = L("Supports");
    def->tooltip = L("Merging bridges or pillars into another pillars can "
                     "increase the radius. Zero means no increase, one means "
                     "full increase.");
    def->min = 0;
    def->max = 1;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloat(0.0));

    def = this->add("support_base_diameter", coFloat);
    def->label = L("Support base diameter");
    def->category = L("Supports");
    def->tooltip = L("Diameter in mm of the pillar base");
    def->sidetext = L("mm");
    def->min = 0;
    def->max = 30;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloat(4.0));

    def = this->add("support_base_height", coFloat);
    def->label = L("Support base height");
    def->category = L("Supports");
    def->tooltip = L("The height of the pillar base cone");
    def->sidetext = L("mm");
    def->min = 0;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloat(1.0));
    
    def = this->add("support_base_safety_distance", coFloat);
    def->label = L("Support base safety distance");
    def->category = L("Supports");
    def->tooltip  = L(
        "The minimum distance of the pillar base from the model in mm. "
        "Makes sense in zero elevation mode where a gap according "
        "to this parameter is inserted between the model and the pad.");
    def->sidetext = L("mm");
    def->min = 0;
    def->max = 10;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloat(1));

    def = this->add("support_critical_angle", coFloat);
    def->label = L("Critical angle");
    def->category = L("Supports");
    def->tooltip = L("The default angle for connecting support sticks and junctions.");
    def->sidetext = L("°");
    def->min = 0;
    def->max = 90;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloat(45));

    def = this->add("support_max_bridge_length", coFloat);
    def->label = L("Max bridge length");
    def->category = L("Supports");
    def->tooltip = L("The max length of a bridge");
    def->sidetext = L("mm");
    def->min = 0;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloat(15.0));

    def = this->add("support_max_pillar_link_distance", coFloat);
    def->label = L("Max pillar linking distance");
    def->category = L("Supports");
    def->tooltip = L("The max distance of two pillars to get linked with each other."
                     " A zero value will prohibit pillar cascading.");
    def->sidetext = L("mm");
    def->min = 0;   // 0 means no linking
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloat(10.0));

    def = this->add("support_object_elevation", coFloat);
    def->label = L("Object elevation");
    def->category = L("Supports");
    def->tooltip = L("How much the supports should lift up the supported object. "
                     "If this value is zero, the bottom of the model geometry "
                     "will be considered as part of the pad.");
    def->sidetext = L("mm");
    def->min = 0;
    def->max = 150; // This is the max height of print on SL1
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloat(5.0));

    def = this->add("support_points_density_relative", coInt);
    def->label = L("Support points density");
    def->category = L("Supports");
    def->tooltip = L("This is a relative measure of support points density.");
    def->sidetext = L("%");
    def->min = 0;
    def->set_default_value(new ConfigOptionInt(100));

    def = this->add("support_points_minimal_distance", coFloat);
    def->label = L("Minimal distance of the support points");
    def->category = L("Supports");
    def->tooltip = L("No support points will be placed closer than this threshold.");
    def->sidetext = L("mm");
    def->min = 0;
    def->set_default_value(new ConfigOptionFloat(1.f));

    def = this->add("pad_enable", coBool);
    def->label = L("Use pad");
    def->category = L("Pad");
    def->tooltip = L("Add a pad underneath the supported model");
    def->mode = comSimple;
    def->set_default_value(new ConfigOptionBool(true));

    def = this->add("pad_wall_thickness", coFloat);
    def->label = L("Pad wall thickness");
    def->category = L("Pad");
     def->tooltip = L("The thickness of the pad and its optional cavity walls.");
    def->sidetext = L("mm");
    def->min = 0;
    def->max = 30;
    def->mode = comSimple;
    def->set_default_value(new ConfigOptionFloat(2.0));

    def = this->add("pad_wall_height", coFloat);
    def->label = L("Pad wall height");
    def->tooltip = L("Defines the pad cavity depth. Set to zero to disable the cavity. "
                     "Be careful when enabling this feature, as some resins may "
                     "produce an extreme suction effect inside the cavity, "
                     "which makes peeling the print off the vat foil difficult.");
    def->category = L("Pad");
//     def->tooltip = L("");
    def->sidetext = L("mm");
    def->min = 0;
    def->max = 30;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloat(0.));

    def = this->add("pad_max_merge_distance", coFloat);
    def->label = L("Max merge distance");
    def->category = L("Pad");
     def->tooltip = L("Some objects can get along with a few smaller pads "
                      "instead of a single big one. This parameter defines "
                      "how far the center of two smaller pads should be. If they"
                      "are closer, they will get merged into one pad.");
    def->sidetext = L("mm");
    def->min = 0;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloat(50.0));

    // This is disabled on the UI. I hope it will never be enabled.
    def = this->add("pad_edge_radius", coFloat);
    def->label = L("Pad edge radius");
    def->category = L("Pad");
//     def->tooltip = L("");
    def->sidetext = L("mm");
    def->min = 0;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloat(1.0));

    def = this->add("pad_wall_slope", coFloat);
    def->label = L("Pad wall slope");
    def->category = L("Pad");
    def->tooltip = L("The slope of the pad wall relative to the bed plane. "
                     "90 degrees means straight walls.");
    def->sidetext = L("°");
    def->min = 45;
    def->max = 90;
    def->mode = comAdvanced;
    def->set_default_value(new ConfigOptionFloat(45.0));
    
    def = this->add("pad_object_gap", coFloat);
    def->label = L("Pad object gap");
    def->category = L("Pad");
    def->tooltip  = L("The gap between the object bottom and the generated "
                      "pad in zero elevation mode.");
    def->sidetext = L("mm");
    def->min = 0;
    def->max = 10;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloat(1));
    
    def = this->add("pad_object_connector_stride", coFloat);
    def->label = L("Pad object connector stride");
    def->category = L("Pad");
    def->tooltip = L("Distance between two connector sticks between "
                     "the object pad and the generated pad.");
    def->sidetext = L("mm");
    def->min = 0;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloat(10));
    
    def = this->add("pad_object_connector_width", coFloat);
    def->label = L("Pad object connector width");
    def->category = L("Pad");
    def->tooltip  = L("The width of the connectors sticks which connect the "
                      "object pad and the generated pad.");
    def->sidetext = L("mm");
    def->min = 0;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloat(0.5));
    
    def = this->add("pad_object_connector_penetration", coFloat);
    def->label = L("Pad object connector penetration");
    def->category = L("Pad");
    def->tooltip  = L(
        "How much should the tiny connectors penetrate into the model body.");
    def->sidetext = L("mm");
    def->min = 0;
    def->mode = comExpert;
    def->set_default_value(new ConfigOptionFloat(0.3));
}

void PrintConfigDef::handle_legacy(t_config_option_key &opt_key, std::string &value)
{
    // handle legacy options
    if (opt_key == "extrusion_width_ratio" || opt_key == "bottom_layer_speed_ratio"
        || opt_key == "first_layer_height_ratio") {
        boost::replace_first(opt_key, "_ratio", "");
        if (opt_key == "bottom_layer_speed") opt_key = "first_layer_speed";
        try {
            float v = boost::lexical_cast<float>(value);
            if (v != 0) 
                value = boost::lexical_cast<std::string>(v*100) + "%";
        } catch (boost::bad_lexical_cast &) {
            value = "0";
        }
    } else if (opt_key == "gcode_flavor" && value == "makerbot") {
        value = "makerware";
    } else if (opt_key == "fill_density" && value.find("%") == std::string::npos) {
        try {
            // fill_density was turned into a percent value
            float v = boost::lexical_cast<float>(value);
            value = boost::lexical_cast<std::string>(v*100) + "%";
        } catch (boost::bad_lexical_cast &) {}
    } else if (opt_key == "randomize_start" && value == "1") {
        opt_key = "seam_position";
        value = "random";
    } else if (opt_key == "bed_size" && !value.empty()) {
        opt_key = "bed_shape";
        ConfigOptionPoint p;
        p.deserialize(value);
        std::ostringstream oss;
        oss << "0x0," << p.value(0) << "x0," << p.value(0) << "x" << p.value(1) << ",0x" << p.value(1);
        value = oss.str();
    } else if ((opt_key == "perimeter_acceleration" && value == "25")
        || (opt_key == "infill_acceleration" && value == "50")) {
        /*  For historical reasons, the world's full of configs having these very low values;
            to avoid unexpected behavior we need to ignore them. Banning these two hard-coded
            values is a dirty hack and will need to be removed sometime in the future, but it
            will avoid lots of complaints for now. */
        value = "0";
    } else if (opt_key == "support_material_pattern" && value == "pillars") {
        // Slic3r PE does not support the pillars. They never worked well.
        value = "rectilinear";
    } else if (opt_key == "octoprint_host") {
        opt_key = "print_host";
    } else if (opt_key == "octoprint_cafile") {
        opt_key = "printhost_cafile";
    } else if (opt_key == "octoprint_apikey") {
        opt_key = "printhost_apikey";
    }
    
    // Ignore the following obsolete configuration keys:
    static std::set<std::string> ignore = {
        "duplicate_x", "duplicate_y", "gcode_arcs", "multiply_x", "multiply_y",
        "support_material_tool", "acceleration", "adjust_overhang_flow", 
        "standby_temperature", "scale", "rotate", "duplicate", "duplicate_grid",
        "start_perimeters_at_concave_points", "start_perimeters_at_non_overhang", "randomize_start", 
        "seal_position", "vibration_limit", "bed_size", 
        "print_center", "g0", "threads", "pressure_advance", "wipe_tower_per_color_wipe"
#ifndef HAS_PRESSURE_EQUALIZER
        , "max_volumetric_extrusion_rate_slope_positive", "max_volumetric_extrusion_rate_slope_negative"
#endif /* HAS_PRESSURE_EQUALIZER */
    };

    if (ignore.find(opt_key) != ignore.end()) {
        opt_key = "";
        return;
    }
    
    if (! print_config_def.has(opt_key)) {
        opt_key = "";
        return;
    }
}

const PrintConfigDef print_config_def;

DynamicPrintConfig* DynamicPrintConfig::new_from_defaults()
{
    return new_from_defaults_keys(FullPrintConfig::defaults().keys());
}

DynamicPrintConfig* DynamicPrintConfig::new_from_defaults_keys(const std::vector<std::string> &keys)
{
    auto *out = new DynamicPrintConfig();
    out->apply_only(FullPrintConfig::defaults(), keys);
    return out;
}

void DynamicPrintConfig::normalize()
{
    if (this->has("extruder")) {
        int extruder = this->option("extruder")->getInt();
        this->erase("extruder");
        if (extruder != 0) {
            if (!this->has("infill_extruder"))
                this->option("infill_extruder", true)->setInt(extruder);
            if (!this->has("perimeter_extruder"))
                this->option("perimeter_extruder", true)->setInt(extruder);
            // Don't propagate the current extruder to support.
            // For non-soluble supports, the default "0" extruder means to use the active extruder,
            // for soluble supports one certainly does not want to set the extruder to non-soluble.
            // if (!this->has("support_material_extruder"))
            //     this->option("support_material_extruder", true)->setInt(extruder);
            // if (!this->has("support_material_interface_extruder"))
            //     this->option("support_material_interface_extruder", true)->setInt(extruder);
        }
    }
    
    if (!this->has("solid_infill_extruder") && this->has("infill_extruder"))
        this->option("solid_infill_extruder", true)->setInt(this->option("infill_extruder")->getInt());
    
    if (this->has("spiral_vase") && this->opt<ConfigOptionBool>("spiral_vase", true)->value) {
        {
            // this should be actually done only on the spiral layers instead of all
            ConfigOptionBools* opt = this->opt<ConfigOptionBools>("retract_layer_change", true);
            opt->values.assign(opt->values.size(), false);  // set all values to false
        }
        {
            this->opt<ConfigOptionInt>("perimeters", true)->value = 1;
            this->opt<ConfigOptionInt>("top_solid_layers", true)->value = 0;
            this->opt<ConfigOptionPercent>("fill_density", true)->value = 0;
            this->opt<ConfigOptionBool>("support_material", true)->value = false;
            this->opt<ConfigOptionInt>("support_material_enforce_layers")->value = 0;
            this->opt<ConfigOptionBool>("exact_last_layer_height", true)->value = false;
            this->opt<ConfigOptionBool>("ensure_vertical_shell_thickness", true)->value = false;
            this->opt<ConfigOptionBool>("infill_dense", true)->value = false;
            this->opt<ConfigOptionBool>("extra_perimeters", true)->value = false;
        }
    }
}

std::string DynamicPrintConfig::validate()
{
    // Full print config is initialized from the defaults.
    const ConfigOption *opt = this->option("printer_technology", false);
    auto printer_technology = (opt == nullptr) ? ptFFF : static_cast<PrinterTechnology>(dynamic_cast<const ConfigOptionEnumGeneric*>(opt)->value);
    switch (printer_technology) {
    case ptFFF:
    {
        FullPrintConfig fpc;
        fpc.apply(*this, true);
        // Verify this print options through the FullPrintConfig.
        return fpc.validate();
    }
    default:
        //FIXME no validation on SLA data?
        return std::string();
    }
}

double PrintConfig::min_object_distance() const
{
    return PrintConfig::min_object_distance(static_cast<const ConfigBase*>(this));
}

double PrintConfig::min_object_distance(const ConfigBase *config)
{
    double base_dist = 0;
    if (config->option("complete_objects")->getBool()){
        std::vector<double> vals = dynamic_cast<const ConfigOptionFloats*>(config->option("nozzle_diameter"))->values;
        double max_nozzle_diam = 0;
        for (double val : vals) max_nozzle_diam = std::fmax(max_nozzle_diam, val);

        // min object distance is max(duplicate_distance, clearance_radius)
        double extruder_clearance_radius = config->option("extruder_clearance_radius")->getFloat();
        if (extruder_clearance_radius > base_dist){
            base_dist = extruder_clearance_radius;
        }
        //add brim width
        if (config->option("brim_width")->getFloat() > 0){
            base_dist += config->option("brim_width")->getFloat() * 2;
        }
        //add the skirt
        if (config->option("skirts")->getInt() > 0){
            //add skirt dist
            double dist_skirt = config->option("skirt_distance")->getFloat();
            if (dist_skirt > config->option("brim_width")->getFloat())
                base_dist += (dist_skirt - config->option("brim_width")->getFloat()) * 2;
            //add skirt width
            base_dist += max_nozzle_diam * config->option("skirts")->getInt() * 1.5 * 2;
        }
    }
    return base_dist;
}

//FIXME localize this function.
std::string FullPrintConfig::validate()
{
    // --layer-height
    if (this->get_abs_value("layer_height") <= 0)
        return "Invalid value for --layer-height";
    if (fabs(fmod(this->get_abs_value("layer_height"), SCALING_FACTOR)) > 1e-4)
        return "--layer-height must be a multiple of print resolution";

    // --first-layer-height
    if (this->get_abs_value("first_layer_height") <= 0)
        return "Invalid value for --first-layer-height";

    // --filament-diameter
    for (double fd : this->filament_diameter.values)
        if (fd < 1)
            return "Invalid value for --filament-diameter";

    // --nozzle-diameter
    for (double nd : this->nozzle_diameter.values)
        if (nd < 0.005)
            return "Invalid value for --nozzle-diameter";
    
    // --perimeters
    if (this->perimeters.value < 0)
        return "Invalid value for --perimeters";

    // --solid-layers
    if (this->top_solid_layers < 0)
        return "Invalid value for --top-solid-layers";
    if (this->bottom_solid_layers < 0)
        return "Invalid value for --bottom-solid-layers";
    
    if (this->use_firmware_retraction.value && 
        this->gcode_flavor.value != gcfSmoothie &&
        this->gcode_flavor.value != gcfRepRap &&
        this->gcode_flavor.value != gcfMarlin &&
        this->gcode_flavor.value != gcfMachinekit &&
        this->gcode_flavor.value != gcfRepetier)
        return "--use-firmware-retraction is only supported by Marlin, Smoothie, Repetier and Machinekit firmware";

    if (this->use_firmware_retraction.value)
        for (unsigned char wipe : this->wipe.values)
             if (wipe)
                return "--use-firmware-retraction is not compatible with --wipe";
        
    // --gcode-flavor
    if (! print_config_def.get("gcode_flavor")->has_enum_value(this->gcode_flavor.serialize()))
        return "Invalid value for --gcode-flavor";
    
    // --fill-pattern
    if (! print_config_def.get("fill_pattern")->has_enum_value(this->fill_pattern.serialize()))
        return "Invalid value for --fill-pattern";

    // --top-fill-pattern
    if (!print_config_def.get("top_fill_pattern")->has_enum_value(this->top_fill_pattern.serialize()))
        return "Invalid value for --top-fill-pattern";

    // --bottom-fill-pattern
    if (! print_config_def.get("bottom_fill_pattern")->has_enum_value(this->bottom_fill_pattern.serialize()))
        return "Invalid value for --bottom-fill-pattern";

    // --solid-fill-pattern
    if (!print_config_def.get("solid_fill_pattern")->has_enum_value(this->solid_fill_pattern.serialize()))
        return "Invalid value for --solid-fill-pattern";

    // --fill-density
    if (fabs(this->fill_density.value - 100.) < EPSILON &&
        (! print_config_def.get("top_fill_pattern")->has_enum_value(this->fill_pattern.serialize())
        || ! print_config_def.get("bottom_fill_pattern")->has_enum_value(this->fill_pattern.serialize())
        ))
        return "The selected fill pattern is not supposed to work at 100% density";
    
    // --infill-every-layers
    if (this->infill_every_layers < 1)
        return "Invalid value for --infill-every-layers";

    // --skirt-height
    if (this->skirt_height < -1) // -1 means as tall as the object
        return "Invalid value for --skirt-height";
    
    // --bridge-flow-ratio
    if (this->bridge_flow_ratio <= 0)
        return "Invalid value for --bridge-flow-ratio";
    
    // --over-bridge-flow-ratio
    if (this->over_bridge_flow_ratio <= 0)
        return "Invalid value for --over-bridge-flow-ratio";

    // --over-bridge-flow-ratio
    if (this->top_infill_flow_ratio <= 0)
        return "Invalid value for --top-infill-flow-ratio";

    // --over-bridge-flow-ratio
    if (this->ironing_flow_ratio <= 0)
        return "Invalid value for --ironing-flow-ratio";
    
    // extruder clearance
    if (this->extruder_clearance_radius <= 0)
        return "Invalid value for --extruder-clearance-radius";
    if (this->extruder_clearance_height <= 0)
        return "Invalid value for --extruder-clearance-height";

    // --extrusion-multiplier
    for (double em : this->extrusion_multiplier.values)
        if (em <= 0)
            return "Invalid value for --extrusion-multiplier";

    // --default-acceleration
    if ((this->perimeter_acceleration != 0. || this->infill_acceleration != 0. || this->bridge_acceleration != 0. || this->first_layer_acceleration != 0.) &&
        this->default_acceleration == 0.)
        return "Invalid zero value for --default-acceleration when using other acceleration settings";

    // --spiral-vase
    if (this->spiral_vase) {
        // Note that we might want to have more than one perimeter on the bottom
        // solid layers.
        if (this->perimeters > 1)
            return "Can't make more than one perimeter when spiral vase mode is enabled";
        else if (this->perimeters < 1)
            return "Can't make less than one perimeter when spiral vase mode is enabled";
        if (this->fill_density > 0)
            return "Spiral vase mode can only print hollow objects, so you need to set Fill density to 0";
        if (this->top_solid_layers > 0)
            return "Spiral vase mode is not compatible with top solid layers";
        if (this->support_material || this->support_material_enforce_layers > 0)
            return "Spiral vase mode is not compatible with support material";
        if (this->infill_dense)
            return "Spiral vase mode can only print hollow objects and have no top surface, so you don't need any dense infill";
        if (this->extra_perimeters)
            return "Can't make more than one perimeter when spiral vase mode is enabled";
    }
    
    // extrusion widths
    {
        double max_nozzle_diameter = 0.;
        for (double dmr : this->nozzle_diameter.values)
            max_nozzle_diameter = std::max(max_nozzle_diameter, dmr);
        const char *widths[] = { "external_perimeter", "perimeter", "infill", "solid_infill", "top_infill", "support_material", "first_layer" };
        for (size_t i = 0; i < sizeof(widths) / sizeof(widths[i]); ++ i) {
            std::string key(widths[i]);
            key += "_extrusion_width";
            if (this->get_abs_value(key, max_nozzle_diameter) > 10. * max_nozzle_diameter)
                return std::string("Invalid extrusion width (too large): ") + key;
        }
    }

    // Out of range validation of numeric values.
    for (const std::string &opt_key : this->keys()) {
        const ConfigOption      *opt    = this->optptr(opt_key);
        assert(opt != nullptr);
        const ConfigOptionDef   *optdef = print_config_def.get(opt_key);
        assert(optdef != nullptr);
        bool out_of_range = false;
        switch (opt->type()) {
        case coFloat:
        case coPercent:
        case coFloatOrPercent:
        {
            auto *fopt = static_cast<const ConfigOptionFloat*>(opt);
            out_of_range = fopt->value < optdef->min || fopt->value > optdef->max;
            break;
        }
        case coFloats:
        case coPercents:
            for (double v : static_cast<const ConfigOptionVector<double>*>(opt)->values)
                if (v < optdef->min || v > optdef->max) {
                    out_of_range = true;
                    break;
                }
            break;
        case coInt:
        {
            auto *iopt = static_cast<const ConfigOptionInt*>(opt);
            out_of_range = iopt->value < optdef->min || iopt->value > optdef->max;
            break;
        }
        case coInts:
            for (int v : static_cast<const ConfigOptionVector<int>*>(opt)->values)
                if (v < optdef->min || v > optdef->max) {
                    out_of_range = true;
                    break;
                }
            break;
        default:;
        }
        if (out_of_range)
            return std::string("Value out of range: " + opt_key);
    }
    
    // The configuration is valid.
    return "";
}

// Declare the static caches for each StaticPrintConfig derived class.
StaticPrintConfig::StaticCache<class Slic3r::PrintObjectConfig> PrintObjectConfig::s_cache_PrintObjectConfig;
StaticPrintConfig::StaticCache<class Slic3r::PrintRegionConfig> PrintRegionConfig::s_cache_PrintRegionConfig;
StaticPrintConfig::StaticCache<class Slic3r::MachineEnvelopeConfig> MachineEnvelopeConfig::s_cache_MachineEnvelopeConfig;
StaticPrintConfig::StaticCache<class Slic3r::GCodeConfig>       GCodeConfig::s_cache_GCodeConfig;
StaticPrintConfig::StaticCache<class Slic3r::PrintConfig>       PrintConfig::s_cache_PrintConfig;
StaticPrintConfig::StaticCache<class Slic3r::HostConfig>        HostConfig::s_cache_HostConfig;
StaticPrintConfig::StaticCache<class Slic3r::FullPrintConfig>   FullPrintConfig::s_cache_FullPrintConfig;

StaticPrintConfig::StaticCache<class Slic3r::SLAMaterialConfig>     SLAMaterialConfig::s_cache_SLAMaterialConfig;
StaticPrintConfig::StaticCache<class Slic3r::SLAPrintConfig>        SLAPrintConfig::s_cache_SLAPrintConfig;
StaticPrintConfig::StaticCache<class Slic3r::SLAPrintObjectConfig>  SLAPrintObjectConfig::s_cache_SLAPrintObjectConfig;
StaticPrintConfig::StaticCache<class Slic3r::SLAPrinterConfig>      SLAPrinterConfig::s_cache_SLAPrinterConfig;
StaticPrintConfig::StaticCache<class Slic3r::SLAFullPrintConfig>    SLAFullPrintConfig::s_cache_SLAFullPrintConfig;

CLIActionsConfigDef::CLIActionsConfigDef()
{
    ConfigOptionDef* def;
    
    // Actions:
    def = this->add("export_obj", coBool);
    def->label = L("Export OBJ");
    def->tooltip = L("Export the model(s) as OBJ.");
    def->set_default_value(new ConfigOptionBool(false));
    
/*
    def = this->add("export_svg", coBool);
    def->label = L("Export SVG");
    def->tooltip = L("Slice the model and export solid slices as SVG.");
    def->set_default_value(new ConfigOptionBool(false);
    def->set_default_value(new ConfigOptionBool(false));
*/
    
    def = this->add("export_sla", coBool);
    def->label = L("Export SLA");
    def->tooltip = L("Slice the model and export SLA printing layers as PNG.");
    def->cli = "export-sla|sla";
    def->set_default_value(new ConfigOptionBool(false));

    def = this->add("export_3mf", coBool);
    def->label = L("Export 3MF");
    def->tooltip = L("Export the model(s) as 3MF.");
    def->set_default_value(new ConfigOptionBool(false));

    def = this->add("export_amf", coBool);
    def->label = L("Export AMF");
    def->tooltip = L("Export the model(s) as AMF.");
    def->set_default_value(new ConfigOptionBool(false));

    def = this->add("export_stl", coBool);
    def->label = L("Export STL");
    def->tooltip = L("Export the model(s) as STL.");
    def->set_default_value(new ConfigOptionBool(false));

    def = this->add("export_gcode", coBool);
    def->label = L("Export G-code");
    def->tooltip = L("Slice the model and export toolpaths as G-code.");
    def->cli = "export-gcode|gcode|g";
    def->set_default_value(new ConfigOptionBool(false));

    def = this->add("slice", coBool);
    def->label = L("Slice");
    def->tooltip = L("Slice the model as FFF or SLA based on the printer_technology configuration value.");
    def->cli = "slice|s";
    def->set_default_value(new ConfigOptionBool(false));

    def = this->add("help", coBool);
    def->label = L("Help");
    def->tooltip = L("Show this help.");
    def->cli = "help|h";
    def->set_default_value(new ConfigOptionBool(false));

    def = this->add("help_fff", coBool);
    def->label = L("Help (FFF options)");
    def->tooltip = L("Show the full list of print/G-code configuration options.");
    def->set_default_value(new ConfigOptionBool(false));

    def = this->add("help_sla", coBool);
    def->label = L("Help (SLA options)");
    def->tooltip = L("Show the full list of SLA print configuration options.");
    def->set_default_value(new ConfigOptionBool(false));
    
    def = this->add("info", coBool);
    def->label = L("Output Model Info");
    def->tooltip = L("Write information about the model to the console.");
    def->set_default_value(new ConfigOptionBool(false));
    
    def = this->add("save", coString);
    def->label = L("Save config file");
    def->tooltip = L("Save configuration to the specified file.");
    def->set_default_value(new ConfigOptionString());
}

CLITransformConfigDef::CLITransformConfigDef()
{
    ConfigOptionDef* def;
    
    // Transform options:
    def = this->add("align_xy", coPoint);
    def->label = L("Align XY");
    def->tooltip = L("Align the model to the given point.");
    def->set_default_value(new ConfigOptionPoint(Vec2d(100,100)));
    
    def = this->add("cut", coFloat);
    def->label = L("Cut");
    def->tooltip = L("Cut model at the given Z.");
    def->set_default_value(new ConfigOptionFloat(0));
    
/*
    def = this->add("cut_grid", coFloat);
    def->label = L("Cut");
    def->tooltip = L("Cut model in the XY plane into tiles of the specified max size.");
    def->set_default_value(new ConfigOptionPoint();
    def->set_default_value(new ConfigOptionPoint());
    
    def = this->add("cut_x", coFloat);
    def->label = L("Cut");
    def->tooltip = L("Cut model at the given X.");
    def->set_default_value(new ConfigOptionFloat(0);
    def->set_default_value(new ConfigOptionFloat(0));
    
    def = this->add("cut_y", coFloat);
    def->label = L("Cut");
    def->tooltip = L("Cut model at the given Y.");
    def->set_default_value(new ConfigOptionFloat(0);
    def->set_default_value(new ConfigOptionFloat(0));
*/
    
    def = this->add("center", coPoint);
    def->label = L("Center");
    def->tooltip = L("Center the print around the given center.");
    def->set_default_value(new ConfigOptionPoint(Vec2d(100,100)));

    def = this->add("dont_arrange", coBool);
    def->label = L("Don't arrange");
    def->tooltip = L("Do not rearrange the given models before merging and keep their original XY coordinates.");
    
    def = this->add("duplicate", coInt);
    def->label = L("Duplicate");
    def->tooltip =L("Multiply copies by this factor.");
    def->min = 1;
    
    def = this->add("duplicate_grid", coPoint);
    def->label = L("Duplicate by grid");
    def->tooltip = L("Multiply copies by creating a grid.");

    def = this->add("merge", coBool);
    def->label = L("Merge");
    def->tooltip = L("Arrange the supplied models in a plate and merge them in a single model in order to perform actions once.");
    def->cli = "merge|m";

    def = this->add("repair", coBool);
    def->label = L("Repair");
    def->tooltip = L("Try to repair any non-manifold meshes (this option is implicitly added whenever we need to slice the model to perform the requested action).");
    
    def = this->add("rotate", coFloat);
    def->label = L("Rotate");
    def->tooltip = L("Rotation angle around the Z axis in degrees.");
    def->set_default_value(new ConfigOptionFloat(0));
    
    def = this->add("rotate_x", coFloat);
    def->label = L("Rotate around X");
    def->tooltip = L("Rotation angle around the X axis in degrees.");
    def->set_default_value(new ConfigOptionFloat(0));
    
    def = this->add("rotate_y", coFloat);
    def->label = L("Rotate around Y");
    def->tooltip = L("Rotation angle around the Y axis in degrees.");
    def->set_default_value(new ConfigOptionFloat(0));
    
    def = this->add("scale", coFloatOrPercent);
    def->label = L("Scale");
    def->tooltip = L("Scaling factor or percentage.");
    def->set_default_value(new ConfigOptionFloatOrPercent(1, false));

    def = this->add("split", coBool);
    def->label = L("Split");
    def->tooltip = L("Detect unconnected parts in the given model(s) and split them into separate objects.");
    
    def = this->add("scale_to_fit", coPoint3);
    def->label = L("Scale to Fit");
    def->tooltip = L("Scale to fit the given volume.");
    def->set_default_value(new ConfigOptionPoint3(Vec3d(0,0,0)));
}

CLIMiscConfigDef::CLIMiscConfigDef()
{
    ConfigOptionDef* def;
    
    def = this->add("ignore_nonexistent_config", coBool);
    def->label = L("Ignore non-existent config files");
    def->tooltip = L("Do not fail if a file supplied to --load does not exist.");
    
    def = this->add("load", coStrings);
    def->label = L("Load config file");
    def->tooltip = L("Load configuration from the specified file. It can be used more than once to load options from multiple files.");
    
    def = this->add("output", coString);
    def->label = L("Output File");
    def->tooltip = L("The file where the output will be written (if not specified, it will be based on the input file).");
    def->cli = "output|o";

/*    
    def = this->add("autosave", coString);
    def->label = L("Autosave");
    def->tooltip = L("Automatically export current configuration to the specified file.");
*/
    
    def = this->add("datadir", coString);
    def->label = L("Data directory");
    def->tooltip = L("Load and store settings at the given directory. This is useful for maintaining different profiles or including configurations from a network storage.");

    def = this->add("loglevel", coInt);
    def->label = L("Logging level");
    def->tooltip = L("Messages with severity lower or eqal to the loglevel will be printed out. 0:trace, 1:debug, 2:info, 3:warning, 4:error, 5:fatal");
    def->min = 0;

#if defined(_MSC_VER) && defined(SLIC3R_GUI)
    def = this->add("sw_renderer", coBool);
    def->label = L("Render with a software renderer");
    def->tooltip = L("Render with a software renderer. The bundled MESA software renderer is loaded instead of the default OpenGL driver.");
    def->min = 0;
#endif /* _MSC_VER */
}

const CLIActionsConfigDef    cli_actions_config_def;
const CLITransformConfigDef  cli_transform_config_def;
const CLIMiscConfigDef       cli_misc_config_def;

DynamicPrintAndCLIConfig::PrintAndCLIConfigDef DynamicPrintAndCLIConfig::s_def;

void DynamicPrintAndCLIConfig::handle_legacy(t_config_option_key &opt_key, std::string &value) const
{
	if (cli_actions_config_def  .options.find(opt_key) == cli_actions_config_def  .options.end() &&
		cli_transform_config_def.options.find(opt_key) == cli_transform_config_def.options.end() &&
		cli_misc_config_def     .options.find(opt_key) == cli_misc_config_def     .options.end()) {
		PrintConfigDef::handle_legacy(opt_key, value);
	}
}

}

#include <cereal/types/polymorphic.hpp>
CEREAL_REGISTER_TYPE(Slic3r::DynamicPrintConfig)
CEREAL_REGISTER_POLYMORPHIC_RELATION(Slic3r::DynamicConfig, Slic3r::DynamicPrintConfig) 
