#include <vector>
#include "plugin.hpp"
#include "widgets.hpp"


struct Kanon : Module {
    enum ParamId {
        COARSE_PARAM,
        FINE_PARAM,

        WAVEFORM_SINE_PARAM,
        WAVEFORM_TRIANGLE_PARAM,
        WAVEFORM_SQUARE_PARAM,
        WAVEFORM_SAW_PARAM,

        MODE_KANON_PARAM,
        MODE_FWDFWD_PARAM,
        MODE_FWDBWD_PARAM,
        MODE_RND_PARAM,

        PARAMS_LEN
    };
    enum InputId {
        VOCT_INPUT,
        FINE_INPUT,
        INPUTS_LEN
    };
    enum OutputId {
        VOICE1_OUTPUT,
        VOICE2_OUTPUT,
        VOICE3_OUTPUT,
        VOICE4_OUTPUT,
        OUTPUTS_LEN
    };
    enum LightId {
        WAVEFORM_SINE_LIGHT,
        WAVEFORM_TRIANGLE_LIGHT,
        WAVEFORM_SQUARE_LIGHT,
        WAVEFORM_SAW_LIGHT,

        MODE_KANON_LIGHT,
        MODE_FWDFWD_LIGHT,
        MODE_FWDBWD_LIGHT,
        MODE_RND_LIGHT,
        LIGHTS_LEN
    };

    float phases[4] = { 0.f, 0.f, 0.f, 0.f };
    float vocts[4] = { 0.f, 0.f, 0.f, 0.f };
    dsp::BooleanTrigger waveshape_trigger;
    dsp::BooleanTrigger mode_trigger;

    enum Waveshape {
        SINE,
        TRIANGLE,
        SQUARE,
        SAW
    };

    enum Mode {
        KANON,
        FWDFWD,
        FWDBWD,
        RND
    };

    // Used only in forward-backward mode
    enum Order {
        RIGHT,
        LEFT
    };

    uint8_t waveshape;
    uint8_t mode;
    uint8_t master_voice;
    uint8_t order;
    float last_voct;

    Kanon() {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
        configParam(COARSE_PARAM, -5.f, 5.f, 0.f, "Pitch (1V/octave)", " Hz", 2, dsp::FREQ_C4);
        configParam(FINE_PARAM, -1.f, 1.f, 0.f, "Fine", " cents", 0.f, 100.f);
        configButton(WAVEFORM_SINE_PARAM, "Sine");
        configButton(WAVEFORM_TRIANGLE_PARAM, "Triangle");
        configButton(WAVEFORM_SQUARE_PARAM, "Square");
        configButton(WAVEFORM_SAW_PARAM, "Sawtooth");
        configButton(MODE_KANON_PARAM, "\"Kanon\" mode");
        configButton(MODE_FWDBWD_PARAM, "Forward-backward mode");
        configButton(MODE_FWDFWD_PARAM, "Forward-forward mode");
        configButton(MODE_RND_PARAM, "Random mode");
        configInput(VOCT_INPUT, "1V/octave");

        configLight(WAVEFORM_SINE_LIGHT);
        configLight(WAVEFORM_TRIANGLE_LIGHT);
        configLight(WAVEFORM_SQUARE_LIGHT);
        configLight(WAVEFORM_SAW_LIGHT);

        configLight(MODE_KANON_LIGHT);
        configLight(MODE_FWDBWD_LIGHT);
        configLight(MODE_FWDFWD_LIGHT);
        configLight(MODE_RND_LIGHT);

        configOutput(VOICE1_OUTPUT, "Voice 1");
        configOutput(VOICE2_OUTPUT, "Voice 2");
        configOutput(VOICE3_OUTPUT, "Voice 3");
        configOutput(VOICE4_OUTPUT, "Voice 4");

        setWaveshape(SINE);
        setMode(KANON);

        master_voice = 0;
        order = RIGHT;
        last_voct = 0.f;
    }

    void setWaveshape(uint8_t w)
    {
        waveshape = w;
        for (uint8_t l = SINE; l <= SAW; l++) lights[l + WAVEFORM_SINE_LIGHT].setBrightness(0.f);
        lights[w + WAVEFORM_SINE_LIGHT].setBrightness(1.f);
    }
    void setMode(uint8_t m)
    {
        mode = m;
        if (m == KANON) master_voice = 0;
        for (uint8_t l = KANON; l <= RND; l++) lights[l + MODE_KANON_LIGHT].setBrightness(0.f);
        lights[m + MODE_KANON_LIGHT].setBrightness(1.f);
    }

    /*==- WAVESHAPE FUNCTIONS -==*/
    // Might look into more efficient ways to evaluate
    // in the future (e.g. recursive oscillators, alike
    // Bogaudio although for some reason they sound weird!).
    inline float sine(float phase)
    {
        return std::sin(2.f * M_PI * phase);
    }

    inline float triangle(float phase)
    {
        return 2.f * std::abs(2.f*(phase - std::floor(phase + 0.5f))) - 1;
    }

    inline float square(float phase)
    {
        return phase > 0.5? 1.f : -1.f;
    }

    inline float saw(float phase)
    {
        return 2.f*phase - 1;
    }
    /*==- ------------------- -==*/

    inline float volt2freq(float v)
    {
        return dsp::FREQ_C4 * std::pow(2, v);
    }

    inline float current_pitch()
    {
        return params[COARSE_PARAM].getValue()
        + params[FINE_PARAM].getValue() / 12.0f;
    }

    void process(const ProcessArgs& args) override
    {
        // handle waveform buttons
        for (uint8_t w = SINE; w <= SAW; w++)
            if (waveshape_trigger.process(params[w + WAVEFORM_SINE_PARAM].getValue())) {
                setWaveshape(w);
                break;
            }
        // handle mode buttons
        for (uint8_t m = KANON; m <= RND; m++)
            if (mode_trigger.process(params[m + MODE_KANON_PARAM].getValue())) {
                setMode(m);
                break;
            }

        /*
        ... if the voltage of master pitch is the same as last frame:

        KANON MODE:
            let voice 4 play voice 3 pitch
            let voice 3 play voice 2 pitch
            let voice 2 play the last voice 1 pitch
            set the last voice 1 pitch to the current input pitch

        FWD-FWD MODE: (assume <master> voice)
            set master voice to current input pitch
            master voice = (master voice + 1) % 4

        FWD-BWD MODE: (assume <master> voice)
            set master voice to current input pitch
            if order=left:
                master voice -= 1
                if master voice = 0: order=right
            else:
                master voice += 1
                if master voice = 3: order=left

        Something to think about is whether we should keep track of what outputs
        are connected and only count those voices to whatever pitch assignment
        algorithm we're using. For now we don't --- I don't think that that's
        as big of a worry as it seems, but in the future we can work on that.
        Maybe even make little lights next to each output to let the user know
        we're thinking about them!
        */

        // process pitch change
        if (inputs[VOCT_INPUT].getVoltage() == last_voct && inputs[VOCT_INPUT].getVoltage() != vocts[master_voice]) {
            switch (mode)
            {
                case KANON: {
                    vocts[3] = vocts[2];
                    vocts[2] = vocts[1];
                    vocts[1] = vocts[master_voice];
                    vocts[master_voice] = inputs[VOCT_INPUT].getVoltage();
                    break;
                }
                case FWDFWD: {
                    master_voice = (master_voice + 1) % 4;
                    vocts[master_voice] = inputs[VOCT_INPUT].getVoltage();
                    break;
                }
                case FWDBWD: {
                    if (order == RIGHT) {
                        master_voice += 1;
                        if (master_voice == 3) order = LEFT;
                    } else {
                        master_voice -= 1;
                        if (master_voice == 0) order = RIGHT;
                    }
                    vocts[master_voice] = inputs[VOCT_INPUT].getVoltage();
                    break;
                }
                case RND: {
                    master_voice = (uint8_t)(random::u32() % 4);
                    vocts[master_voice] = inputs[VOCT_INPUT].getVoltage();
                    break;
                }
            }
        }

        for (uint8_t voice = 0; voice < 4; voice++) {
            phases[voice] += volt2freq(current_pitch() + vocts[voice]) * args.sampleTime;
            if (phases[voice] >= 1.f)
                phases[voice] -= 1.f;
        }

        last_voct = inputs[VOCT_INPUT].getVoltage();

        // maybe there's an objectively faster way to do this?
        // this should be fast enough though
        float (Kanon::*waveshapes[])(float) = {&Kanon::sine, &Kanon::triangle, &Kanon::square, &Kanon::saw};

        outputs[VOICE1_OUTPUT].setVoltage(5.f * (this->*waveshapes[waveshape])(phases[0]));
        outputs[VOICE2_OUTPUT].setVoltage(5.f * (this->*waveshapes[waveshape])(phases[1]));
        outputs[VOICE3_OUTPUT].setVoltage(5.f * (this->*waveshapes[waveshape])(phases[2]));
        outputs[VOICE4_OUTPUT].setVoltage(5.f * (this->*waveshapes[waveshape])(phases[3]));
    }

    json_t *dataToJson() override
    {
        json_t *rootJ = json_object();

        json_object_set_new(rootJ, "mode", json_integer(mode));
        json_object_set_new(rootJ, "waveshape", json_integer(waveshape));
        json_object_set_new(rootJ, "coarse", json_real(params[COARSE_PARAM].getValue()));
        json_object_set_new(rootJ, "fine", json_real(params[FINE_PARAM].getValue()));

        return rootJ;
    }

    void dataFromJson(json_t *rootJ) override
    {
        setMode(json_integer_value(json_object_get(rootJ, "mode")));
        setWaveshape(json_integer_value(json_object_get(rootJ, "waveshape")));
        params[COARSE_PARAM].setValue(json_real_value(json_object_get(rootJ, "coarse")));
        params[FINE_PARAM].setValue(json_real_value(json_object_get(rootJ, "fine")));
    }

    void onRandomize() override
    {
        setMode(random::u32() % 4);
        setWaveshape(random::u32() % 4);
    }
};


struct KanonWidget : ModuleWidget {
    KanonWidget(Kanon* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/kanon.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        
        // waveform lights
        addChild(createLightCentered<SmallLight<WhiteLight>>(mm2px(Vec(26.65, 77.65)), module, Kanon::WAVEFORM_SINE_LIGHT));
        addChild(createLightCentered<SmallLight<WhiteLight>>(mm2px(Vec(32.45, 77.65)), module, Kanon::WAVEFORM_TRIANGLE_LIGHT));
        addChild(createLightCentered<SmallLight<WhiteLight>>(mm2px(Vec(38.45, 77.65)), module, Kanon::WAVEFORM_SQUARE_LIGHT));
        addChild(createLightCentered<SmallLight<WhiteLight>>(mm2px(Vec(44.45, 77.65)), module, Kanon::WAVEFORM_SAW_LIGHT));
        // mode lights
        addChild(createLightCentered<SmallLight<WhiteLight>>(mm2px(Vec(26.65, 60.65)), module, Kanon::MODE_KANON_LIGHT));
        addChild(createLightCentered<SmallLight<WhiteLight>>(mm2px(Vec(32.45, 60.65)), module, Kanon::MODE_FWDFWD_LIGHT));
        addChild(createLightCentered<SmallLight<WhiteLight>>(mm2px(Vec(38.45, 60.65)), module, Kanon::MODE_FWDBWD_LIGHT));
        addChild(createLightCentered<SmallLight<WhiteLight>>(mm2px(Vec(44.45, 60.65)), module, Kanon::MODE_RND_LIGHT));

        // knobs
        addParam(createParamCentered<MF_JumboKnob>(mm2px(Vec(25.4, 32.13)), module, Kanon::COARSE_PARAM));
        addParam(createParamCentered<MF_MediumKnob>(mm2px(Vec(11.91, 66.88)), module, Kanon::FINE_PARAM));
        // waveform buttons
        addParam(createParamCentered<MF_MiniButton>(mm2px(Vec(26.65, 77.65)), module, Kanon::WAVEFORM_SINE_PARAM));
        addParam(createParamCentered<MF_MiniButton>(mm2px(Vec(32.45, 77.65)), module, Kanon::WAVEFORM_TRIANGLE_PARAM));
        addParam(createParamCentered<MF_MiniButton>(mm2px(Vec(38.45, 77.65)), module, Kanon::WAVEFORM_SQUARE_PARAM));
        addParam(createParamCentered<MF_MiniButton>(mm2px(Vec(44.45, 77.65)), module, Kanon::WAVEFORM_SAW_PARAM));
        // mode buttons
        addParam(createParamCentered<MF_MiniButton>(mm2px(Vec(26.65, 60.65)), module, Kanon::MODE_KANON_PARAM));
        addParam(createParamCentered<MF_MiniButton>(mm2px(Vec(32.45, 60.65)), module, Kanon::MODE_FWDFWD_PARAM));
        addParam(createParamCentered<MF_MiniButton>(mm2px(Vec(38.45, 60.65)), module, Kanon::MODE_FWDBWD_PARAM));
        addParam(createParamCentered<MF_MiniButton>(mm2px(Vec(44.45, 60.65)), module, Kanon::MODE_RND_PARAM));

        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(11.91, 108.31)), module, Kanon::VOCT_INPUT));

        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(29.27, 93.95)), module, Kanon::VOICE1_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(41.62, 93.95)), module, Kanon::VOICE2_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(29.27, 108.31)), module, Kanon::VOICE3_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(41.62, 108.31)), module, Kanon::VOICE4_OUTPUT));
    }
};


Model* modelKanon = createModel<Kanon, KanonWidget>("kanon");