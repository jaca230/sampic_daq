#ifndef SAMPIC_DAQ_CORE_REGISTRY_MODE_AUTO_REGISTRATION_H
#define SAMPIC_DAQ_CORE_REGISTRY_MODE_AUTO_REGISTRATION_H

#ifndef SAMPIC_CONCAT
#define SAMPIC_CONCAT_IMPL(left, right) left##right
#define SAMPIC_CONCAT(left, right) SAMPIC_CONCAT_IMPL(left, right)
#endif

#define SAMPIC_REGISTER_MODE(Registry, Mode, Config, Id, Description, Validate) \
    static const Registry::Registration<Mode, Config>                          \
        SAMPIC_CONCAT(sampic_mode_registration_, __COUNTER__){                 \
            Id, Description, Config{}, Validate}

#endif
