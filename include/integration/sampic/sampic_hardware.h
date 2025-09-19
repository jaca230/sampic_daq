#ifndef SAMPIC_HARDWARE_H
#define SAMPIC_HARDWARE_H

#include "integration/sampic/sampic_configurator.h"
// Sampic
extern "C" {
#include <SAMPIC_256Ch_lib.h>
#include <SAMPIC_256Ch_Type.h>
}

#include <string>

class SampicHardware {
public:
    SampicHardware();

    void initialize(const SampicSystemSettings& settings);
    void applySettings(const SampicSystemSettings& settings);

    CrateInfoStruct& info() { return info_; }
    CrateParamStruct& params() { return params_; }
    void* eventBuffer() { return eventBuffer_; }
    ML_Frame* mlFrames() { return mlFrames_; }
    EventStruct& event() { return event_; }

private:
    CrateInfoStruct info_{};
    CrateParamStruct params_{};
    void* eventBuffer_{nullptr};
    ML_Frame* mlFrames_{nullptr};
    EventStruct event_{};

    SampicConfigurator configurator_;
};

#endif // SAMPIC_HARDWARE_H
