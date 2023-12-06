#include <rack.hpp>

struct MF_JumboKnob : RoundKnob {
    MF_JumboKnob() {
        setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/jumboknob.svg")));
        bg->setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/jumboknob-bg.svg")));
    }
};

struct MF_MediumKnob : RoundKnob {
    MF_MediumKnob() {
        setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/mediumknob.svg")));
        bg->setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/mediumknob-bg.svg")));
    }
};

struct MF_MiniButton : SvgSwitch {
    MF_MiniButton() {
        momentary = true;
        addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/minibutton-0.svg")));
        addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/components/minibutton-1.svg")));
    }
};

// For whatever reason this doesn't render properly, i.e. looks like the box size
// is wrong, even though the svg is the same size as the default PJ301M port which
// we're fallbacking to now. Wanted to fix this but decided to focus on the more important
// stuff first but still looking to use this!
struct MF_Port : SvgPort {
    MF_Port() {
        setSvg(Svg::load(asset::plugin(pluginInstance, "res/components/port.svg")));
    }
};