#include <utility>
#include <vector>
#include "plugin.hpp"
#include "widgets.hpp"

#define DELAY_MEMORY_SIZE 3

struct Terminal : Module {
    enum ParamId {
        GAIN1_PARAM,
        GAIN2_PARAM,
        GAIN3_PARAM,

        DELAY1_PARAM,
        DELAY2_PARAM,
        DELAY3_PARAM,

        KILL1_PARAM,
        KILL2_PARAM,
        KILL3_PARAM,
        PARAMS_LEN
    };

    enum InputId {
        INPUT_L_INPUT,
        INPUT_R_INPUT,

        ARRIVAL1_L_INPUT,
        ARRIVAL1_R_INPUT,

        ARRIVAL2_L_INPUT,
        ARRIVAL2_R_INPUT,

        ARRIVAL3_L_INPUT,
        ARRIVAL3_R_INPUT,

        GAIN1_MOD_INPUT,
        GAIN2_MOD_INPUT,
        GAIN3_MOD_INPUT,

        DELAY1_MOD_INPUT,
        DELAY2_MOD_INPUT,
        DELAY3_MOD_INPUT,
        INPUTS_LEN
    };

    enum OutputId {
        DEPARTURE1_L_OUTPUT,
        DEPARTURE1_R_OUTPUT,

        DEPARTURE2_L_OUTPUT,
        DEPARTURE2_R_OUTPUT,

        DEPARTURE3_L_OUTPUT,
        DEPARTURE3_R_OUTPUT,

        OUTPUTS_LEN
    };
    enum LightsId {
        LIGHTS_LEN
    };

    struct Channel {
        // pair for stereo signal
        std::pair<std::vector<float>, std::vector<float>> memory;
        size_t write;
    };
    Channel channels[3];
    size_t memory_size;

    dsp::BooleanTrigger kill_trigger;

    Terminal()
    {
        config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);

        configInput(INPUT_L_INPUT, "Stereo left / mono input");
        configInput(INPUT_R_INPUT, "Stereo right input");

        for (uint8_t i = 0; i < 3; i++) {
            configParam(GAIN1_PARAM + i, 0.f, 1.f, 0.5f, string::f("Channel %d gain", i+1), "%", 0, 100);
            configParam(DELAY1_PARAM + i, 0.f, (float)DELAY_MEMORY_SIZE, 1.5f, string::f("Channel %d delay time", i+1), " s");

            configButton(KILL1_PARAM + i, string::f("Kill channel %d", i+1));

            configInput(ARRIVAL1_L_INPUT + i*2, string::f("Channel %d stereo left feedback input", i+1));
            configInput(ARRIVAL1_R_INPUT + i*2, string::f("Channel %d stereo right feedback input", i+1));
            configInput(GAIN1_MOD_INPUT + i, string::f("Channel %d gain CV input", i+1));
            configInput(DELAY1_MOD_INPUT + i, string::f("Channel %d delay CV input", i+1));

            configOutput(DEPARTURE1_L_OUTPUT + i*2, string::f("Channel %d stereo left feedback output", i+1));
            configOutput(DEPARTURE1_R_OUTPUT + i*2, string::f("Channel %d stereo right feedback output", i+1));

            channels[i].write = 0;
            memory_size = DELAY_MEMORY_SIZE * APP->engine->getSampleRate();
            channels[i].memory = { std::vector<float>(memory_size, 0.f), std::vector<float>(memory_size, 0.f) };
        }
    }

    void process(const ProcessArgs& args) override
    {
    // TODO: Is this modulation calculation correct?
    #define DELAY_TIME clamp(params[DELAY1_PARAM + i].getValue() + (inputs[DELAY1_MOD_INPUT + i].getVoltage()) * 3.f/10.f, 0.f, DELAY_MEMORY_SIZE*args.sampleRate)
    #define GAIN       clamp(params[GAIN1_PARAM + i].getValue() + (inputs[GAIN1_MOD_INPUT + i].getVoltage()) / 10.f, 0.f, 1.f)
        

        // We implement the singly-tapped delay line with a ring buffer,
        // whose size is set specifically to the number of samples in 3
        // seconds of audio, i.e. the maximum delay time.
        // TODO: implement behavior on sample rate changes
        // We can view the delay "memory" in two ways:
        //
        // 1. A static, (<max delay time> * <samplerate> Hz)-size container where each new sample,
        //    the contents of the entire array move to make space for the new value.
        //    In that model, the delay tap, dependent on delay time, is static in one place.
        // 2. (How it actually works) A (3 seconds * <samplerate> Hz)-size ring buffer where each frame,
        //    if the container is full, the oldest element is overwritten with the value of the new sample
        //    and our tap pointer is moved one element forward and also moved accordingly to the delay time
        //    changes.
        // The kill switches not only clear the departure output but also the delay memory!

        for (int i = 0; i < 3; i++) {
            Channel& chan = channels[i];
            chan.memory.first[chan.write]  = inputs[ARRIVAL1_L_INPUT + i*2].getVoltage();
            chan.memory.second[chan.write] = inputs[ARRIVAL1_R_INPUT + i*2].getVoltage();
            
            // how far back in the ring buffer we have to go to reach the appropriate delayed sample
            size_t setback = (size_t)roundf(args.sampleRate * DELAY_TIME);

            size_t delay_location = (chan.write - setback + memory_size) % memory_size;

            outputs[DEPARTURE1_L_OUTPUT + i*2].setVoltage(inputs[INPUT_L_INPUT].getVoltage() + chan.memory.first[delay_location] * GAIN);
            outputs[DEPARTURE1_R_OUTPUT + i*2].setVoltage(inputs[INPUT_R_INPUT].getVoltage() + chan.memory.second[delay_location] * GAIN);

            chan.write++;
            chan.write %= memory_size;

            // TODO: this should be more performant. As it is, holding down the button makes
            // TODO: unpleasant distorted noises.
            if (kill_trigger.process(params[KILL1_PARAM + i].getValue())) {
                outputs[DEPARTURE1_L_OUTPUT + i*2].setVoltage(0.f);
                outputs[DEPARTURE1_R_OUTPUT + i*2].setVoltage(0.f);
                // TODO: ensure is this correct
                std::memset(static_cast<void*>(chan.memory.first.data()), 0, sizeof(float) * memory_size);
                std::memset(static_cast<void*>(chan.memory.second.data()), 0, sizeof(float) * memory_size);
                chan.write = 0;
            }

        }

    #undef GAIN
    #undef DELAY_TIME
    }
};

struct TerminalWidget : ModuleWidget {
    TerminalWidget(Terminal* module) {
        setModule(module);
        setPanel(createPanel(asset::plugin(pluginInstance, "res/panels/terminal.svg")));

        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

        // knobs
        addParam(createParamCentered<MF_MediumKnob>(mm2px(Vec(82.21, 18.39)), module, Terminal::GAIN1_PARAM));
        addParam(createParamCentered<MF_MediumKnob>(mm2px(Vec(82.21, 35.53)), module, Terminal::DELAY1_PARAM));

        addParam(createParamCentered<MF_MediumKnob>(mm2px(Vec(82.21, 55.37)), module, Terminal::GAIN2_PARAM));
        addParam(createParamCentered<MF_MediumKnob>(mm2px(Vec(82.21, 72.5)), module, Terminal::DELAY2_PARAM));

        addParam(createParamCentered<MF_MediumKnob>(mm2px(Vec(82.21, 92.34)), module, Terminal::GAIN3_PARAM));
        addParam(createParamCentered<MF_MediumKnob>(mm2px(Vec(82.21, 109.47)), module, Terminal::DELAY3_PARAM));

        // buttons
        addParam(createParamCentered<MF_MiniButton>(mm2px(Vec(44.5, 43.65)), module, Terminal::KILL1_PARAM));
        addParam(createParamCentered<MF_MiniButton>(mm2px(Vec(44.5, 80.54)), module, Terminal::KILL2_PARAM));
        addParam(createParamCentered<MF_MiniButton>(mm2px(Vec(44.5, 117.52)), module, Terminal::KILL3_PARAM));

        // inputs
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.07, 60.29)), module, Terminal::INPUT_L_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(8.07, 70.65)), module, Terminal::INPUT_R_INPUT));

        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(58.21, 23.31)), module, Terminal::ARRIVAL1_L_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(58.21, 33.67)), module, Terminal::ARRIVAL1_R_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(58.21, 60.29)), module, Terminal::ARRIVAL2_L_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(58.21, 70.65)), module, Terminal::ARRIVAL2_R_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(58.21, 97.26)), module, Terminal::ARRIVAL3_L_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(58.21, 107.62)), module, Terminal::ARRIVAL3_R_INPUT));

        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(94.29, 18.46)), module, Terminal::GAIN1_MOD_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(94.29, 35.6)), module, Terminal::DELAY1_MOD_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(94.29, 55.44)), module, Terminal::GAIN2_MOD_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(94.29, 72.58)), module, Terminal::DELAY2_MOD_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(94.29, 92.41)), module, Terminal::GAIN3_MOD_INPUT));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(94.29, 109.55)), module, Terminal::DELAY3_MOD_INPUT));
        
        // outputs
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(33.61, 23.31)), module, Terminal::DEPARTURE1_L_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(33.61, 33.67)), module, Terminal::DEPARTURE1_R_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(33.61, 60.29)), module, Terminal::DEPARTURE2_L_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(33.61, 70.65)), module, Terminal::DEPARTURE2_R_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(33.61, 97.26)), module, Terminal::DEPARTURE3_L_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(33.61, 107.62)), module, Terminal::DEPARTURE3_R_OUTPUT));
    }
};

Model* modelTerminal = createModel<Terminal, TerminalWidget>("terminal");