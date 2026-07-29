#ifndef SAMPIC_DAQ_CORE_REGISTRY_MODE_REGISTRY_H
#define SAMPIC_DAQ_CORE_REGISTRY_MODE_REGISTRY_H

#include <algorithm>
#include <concepts>
#include <functional>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <rfl/json.hpp>

#include "core/config/config_store.h"
#include "core/registry/mode/mode_descriptor.h"

/// Runtime registry for a family of named strategies. Factories receive the
/// owning subsystem's construction context plus the mode's typed configuration.
template <typename Base, typename Context>
class ModeRegistry {
public:
    using Descriptor = ModeDescriptor<Base, Context>;

    /// Process-wide catalog for this exact Base/Context mode family.
    ///
    /// Each template specialization owns a different catalog, so a mode for one
    /// subsystem cannot be registered with another subsystem's registry.
    static ModeRegistry& catalog() {
        static ModeRegistry registry;
        return registry;
    }

    /// Uniform self-registration for a typed concrete mode.
    ///
    /// Concrete modes only provide their metadata and validation. Construction
    /// is standardized as Mode(Context&, Config), avoiding a repeated factory
    /// function in every mode implementation.
    template <typename Mode, typename Config>
    class Registration {
    public:
        Registration(std::string id,
                     std::string description,
                     Config defaults,
                     std::function<void(const Config&)> validate) {
            static_assert(
                std::derived_from<Mode, Base>,
                "a registered mode must derive from this registry's base mode");
            static_assert(
                std::constructible_from<Mode, Context&, Config>,
                "a registered mode must be constructible from Context& and Config");

            ModeRegistry::catalog().template registerTyped<Config>(
                std::move(id), std::move(description), std::move(defaults),
                std::move(validate),
                [](Context& context, Config config) {
                    return std::make_unique<Mode>(
                        context, std::move(config));
                });
        }
    };

    void registerMode(Descriptor descriptor) {
        if (descriptor.id.empty()) {
            throw std::invalid_argument("mode id is required");
        }
        if (!descriptor.initialize || !descriptor.create) {
            throw std::invalid_argument("mode '" + descriptor.id +
                                        "' requires ODB and factory callbacks");
        }
        const auto id = descriptor.id;
        if (!descriptors_.emplace(id, std::move(descriptor)).second) {
            throw std::logic_error("duplicate mode registration: '" + id + "'");
        }
    }

    template <typename Config, typename Factory>
    void registerTyped(std::string id,
                       std::string description,
                       Config defaults,
                       std::function<void(const Config&)> validate,
                       Factory factory) {
        const auto encode = [](const Config& config) {
            return ConfigStore::Json::parse(rfl::json::write(config));
        };
        const auto decode = [](const ConfigStore::Json& json,
                               const std::string& path) {
            const auto parsed = rfl::json::read<Config>(json.dump());
            if (!parsed.has_value()) {
                throw std::runtime_error("invalid typed mode configuration at '" +
                                         path + "'");
            }
            return parsed.value();
        };
        registerMode(Descriptor{
            std::move(id),
            std::move(description),
            [defaults = std::move(defaults), encode](ConfigStore& store,
                                                     const std::string& path) {
                store.initializeJson(path, encode(defaults));
            },
            [validate = std::move(validate), factory = std::move(factory), decode](
                Context& context, const ConfigStore& store, const std::string& path) {
                Config config = decode(store.readJson(path), path);
                try {
                    if (validate) validate(config);
                } catch (const std::exception& error) {
                    throw std::invalid_argument("invalid mode configuration at '" +
                                                path + "': " + error.what());
                }
                return factory(context, std::move(config));
            }});
    }

    void initializeOdb(ConfigStore& store, const std::string& modes_root) const {
        for (const auto& id : ids()) {
            descriptors_.at(id).initialize(store, modePath(modes_root, id));
        }
    }

    std::unique_ptr<Base> create(std::string_view id,
                                 Context& context,
                                 const ConfigStore& store,
                                 const std::string& modes_root) const {
        const auto it = descriptors_.find(std::string(id));
        if (it == descriptors_.end()) {
            std::ostringstream message;
            message << "unknown mode '" << id << "'; registered modes:";
            for (const auto& registered : ids()) message << ' ' << registered;
            throw std::invalid_argument(message.str());
        }
        return it->second.create(context, store, modePath(modes_root, id));
    }

    std::vector<std::string> ids() const {
        std::vector<std::string> result;
        result.reserve(descriptors_.size());
        for (const auto& [id, ignored] : descriptors_) result.push_back(id);
        std::sort(result.begin(), result.end());
        return result;
    }

private:
    static std::string modePath(const std::string& root, std::string_view id) {
        return root + "/" + std::string(id);
    }

    std::unordered_map<std::string, Descriptor> descriptors_;
};

#endif
