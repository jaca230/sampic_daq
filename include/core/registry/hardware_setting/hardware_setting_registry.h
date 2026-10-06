#ifndef SAMPIC_DAQ_CORE_REGISTRY_HARDWARE_SETTING_REGISTRY_H
#define SAMPIC_DAQ_CORE_REGISTRY_HARDWARE_SETTING_REGISTRY_H

#include "core/config/config_store.h"
#include "core/registry/hardware_setting/hardware_address.h"
#include "core/registry/hardware_setting/hardware_apply_stats.h"
#include "core/registry/hardware_setting/hardware_level.h"
#include "core/registry/hardware_setting/hardware_setting_descriptor.h"
#include "core/registry/hardware_setting/hardware_topology.h"

#include <algorithm>
#include <cmath>
#include <chrono>
#include <functional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include <rfl/json.hpp>

template <typename VendorContext>
class HardwareSettingRegistry {
public:
    using Descriptor = HardwareSettingDescriptor<VendorContext>;

    /// Process-wide catalog for this exact vendor context.
    static HardwareSettingRegistry& catalog() {
        static HardwareSettingRegistry registry;
        return registry;
    }

    /// Uniform self-registration for a provider of related settings.
    template <typename Provider>
    class Registration {
    public:
        Registration() {
            static_assert(
                requires(HardwareSettingRegistry& registry) {
                    Provider::registerSettings(registry);
                },
                "a hardware setting provider must expose "
                "registerSettings(HardwareSettingRegistry&)");
            Provider::registerSettings(HardwareSettingRegistry::catalog());
        }
    };

    template <typename T, typename Validate, typename Getter, typename Setter>
    void registerSetting(std::string id,
                         HardwareLevel level,
                         std::string odb_key,
                         T default_value,
                         int priority,
                         Validate validate,
                         Getter getter,
                         Setter setter) {
        const auto encode = [](const T& value) {
            return ConfigStore::Json::parse(rfl::json::write(value));
        };
        const auto decode = [](const ConfigStore::Json& json) {
            const auto parsed = rfl::json::read<T>(json.dump());
            if (!parsed.has_value()) {
                throw std::invalid_argument(
                    "value does not match the registered setting type");
            }
            return parsed.value();
        };
        Descriptor descriptor{
            std::move(id), level, std::move(odb_key), priority,
            encode(default_value),
            [validate = std::move(validate), decode](const ConfigStore::Json& value) {
                validate(decode(value));
            },
            [getter = std::move(getter), encode](
                const HardwareAddress& address, VendorContext& context) {
                return encode(getter(address, context));
            },
            [setter = std::move(setter), decode](
                const HardwareAddress& address,
                const ConfigStore::Json& value,
                VendorContext& context) {
                setter(address, decode(value), context);
            },
            equivalentJson};
        registerDescriptor(std::move(descriptor));
    }

    void registerDescriptor(Descriptor descriptor) {
        if (descriptor.id.empty() || descriptor.odb_key.empty() ||
            !descriptor.validate || !descriptor.getter || !descriptor.setter ||
            !descriptor.equivalent) {
            throw std::invalid_argument("hardware setting registration is incomplete");
        }
        // MIDAS limits each ODB key component to NAME_LENGTH (32) bytes,
        // including its terminating null byte.
        if (descriptor.odb_key.size() >= 32) {
            throw std::invalid_argument(
                "hardware setting '" + descriptor.id + "' has ODB key '" +
                descriptor.odb_key + "' with " +
                std::to_string(descriptor.odb_key.size()) +
                " bytes; MIDAS requires fewer than 32");
        }
        const std::string path_key = std::to_string(static_cast<int>(descriptor.level)) +
                                     ":" + descriptor.odb_key;
        if (!ids_.insert(descriptor.id).second) {
            throw std::logic_error("duplicate hardware setting id '" + descriptor.id + "'");
        }
        if (!paths_.insert(path_key).second) {
            ids_.erase(descriptor.id);
            throw std::logic_error("duplicate hardware setting path '" +
                                   descriptor.odb_key + "'");
        }
        descriptors_.push_back(std::move(descriptor));
        std::stable_sort(descriptors_.begin(), descriptors_.end(),
            [](const Descriptor& left, const Descriptor& right) {
                if (left.level != right.level) return left.level < right.level;
                if (left.priority != right.priority) return left.priority < right.priority;
                return left.id < right.id;
            });
    }

    void initializeOdb(ConfigStore& store,
                       const std::string& hardware_root,
                       HardwareTopology topology = {}) const {
        for (const auto& descriptor : descriptors_) {
            for (const auto& address : addresses(descriptor.level, topology)) {
                store.initializeJson(path(hardware_root, descriptor, address),
                                     descriptor.default_value);
            }
        }
    }

    void apply(const ConfigStore& store,
               const std::string& hardware_root,
               VendorContext& context,
               HardwareTopology topology) const {
        applyImpl(store, hardware_root, context, topology, nullptr);
    }

    void applySelected(const ConfigStore& store,
                       const std::string& hardware_root,
                       VendorContext& context,
                       HardwareTopology topology,
                       const std::unordered_set<std::string>& ids) const {
        for (const auto& id : ids) find(id);
        applyImpl(store, hardware_root, context, topology, &ids);
    }

    HardwareApplyStats lastApplyStats() const { return last_apply_stats_; }

private:
    void applyImpl(const ConfigStore& store,
                   const std::string& hardware_root,
                   VendorContext& context,
                   HardwareTopology topology,
                   const std::unordered_set<std::string>* selected) const {
        struct Pending {
            const Descriptor* descriptor;
            HardwareAddress address;
            ConfigStore::Json value;
        };
        std::vector<Pending> pending;
        for (const auto& descriptor : descriptors_) {
            if (selected && !selected->contains(descriptor.id)) continue;
            for (const auto& address : addresses(descriptor.level, topology)) {
                if (!ancestorsEnabled(store, hardware_root, descriptor.level, address,
                                      descriptor.odb_key != "enabled")) {
                    continue;
                }
                const auto value = store.readJson(path(hardware_root, descriptor, address));
                try {
                    descriptor.validate(value);
                } catch (const std::exception& error) {
                    throw std::invalid_argument("invalid hardware setting '" + descriptor.id +
                        "' at " + address.describe() + " (value=" + value.dump() +
                        "): " + error.what());
                }
                pending.push_back(Pending{&descriptor, address, value});
            }
        }
        std::size_t changed = 0;
        const auto started = std::chrono::steady_clock::now();
        for (const auto& item : pending) {
            const auto current = item.descriptor->getter(item.address, context);
            if (!item.descriptor->equivalent(current, item.value)) {
                item.descriptor->setter(item.address, item.value, context);
                ++changed;
            }
        }
        last_apply_stats_ = HardwareApplyStats{
            pending.size(), changed,
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - started)};
    }

    const Descriptor* find(std::string_view id) const {
        const auto it = std::find_if(descriptors_.begin(), descriptors_.end(),
            [&](const Descriptor& descriptor) { return descriptor.id == id; });
        if (it == descriptors_.end()) {
            throw std::invalid_argument("unknown hardware setting '" + std::string(id) + "'");
        }
        return &*it;
    }

    static std::vector<HardwareAddress> addresses(
        HardwareLevel level, HardwareTopology topology) {
        std::vector<HardwareAddress> result;
        if (level == HardwareLevel::Crate) return {HardwareAddress{}};
        for (int feb = 0; feb < topology.febs; ++feb) {
            if (level == HardwareLevel::Feb) {
                result.push_back({feb, -1, -1});
                continue;
            }
            for (int chip = 0; chip < topology.chips_per_feb; ++chip) {
                if (level == HardwareLevel::Chip) {
                    result.push_back({feb, chip, -1});
                    continue;
                }
                for (int channel = 0; channel < topology.channels_per_chip; ++channel) {
                    result.push_back({feb, chip, channel});
                }
            }
        }
        return result;
    }

    static std::string path(const std::string& root,
                            const Descriptor& descriptor,
                            const HardwareAddress& address) {
        switch (descriptor.level) {
            case HardwareLevel::Crate:
                return root + "/" + descriptor.odb_key;
            case HardwareLevel::Feb:
                return root + "/front_end_boards/feb" +
                       std::to_string(address.feb) + "/" +
                       descriptor.odb_key;
            case HardwareLevel::Chip:
                return root + "/front_end_boards/feb" +
                       std::to_string(address.feb) +
                       "/sampics/sampic" + std::to_string(address.chip) + "/" +
                       descriptor.odb_key;
            case HardwareLevel::Channel:
                return root + "/front_end_boards/feb" +
                       std::to_string(address.feb) +
                       "/sampics/sampic" + std::to_string(address.chip) +
                       "/channels/channel" + std::to_string(address.channel) +
                       "/" + descriptor.odb_key;
        }
        throw std::logic_error("unknown hardware level");
    }

    static bool ancestorsEnabled(const ConfigStore& store,
                                 const std::string& root,
                                 HardwareLevel level,
                                 const HardwareAddress& address,
                                 bool include_self) {
        if (level >= HardwareLevel::Feb &&
            (level != HardwareLevel::Feb || include_self)) {
            const auto feb_enabled =
                store.readJson(
                    root + "/front_end_boards/feb" +
                    std::to_string(address.feb) + "/enabled");
            if (!feb_enabled.is_null() && !feb_enabled.template get<bool>()) return false;
        }
        if (level >= HardwareLevel::Chip &&
            (level != HardwareLevel::Chip || include_self)) {
            const auto chip_enabled =
                store.readJson(root + "/front_end_boards/feb" +
                               std::to_string(address.feb) +
                               "/sampics/sampic" + std::to_string(address.chip) +
                               "/enabled");
            if (!chip_enabled.is_null() && !chip_enabled.template get<bool>()) return false;
        }
        if (level == HardwareLevel::Channel && include_self) {
            const auto channel_enabled =
                store.readJson(root + "/front_end_boards/feb" +
                               std::to_string(address.feb) +
                               "/sampics/sampic" + std::to_string(address.chip) +
                               "/channels/channel" + std::to_string(address.channel) +
                               "/enabled");
            if (!channel_enabled.is_null() && !channel_enabled.template get<bool>())
                return false;
        }
        return true;
    }

    std::vector<Descriptor> descriptors_;
    std::unordered_set<std::string> ids_;
    std::unordered_set<std::string> paths_;
    mutable HardwareApplyStats last_apply_stats_{};

    static bool equivalentJson(const ConfigStore::Json& left,
                               const ConfigStore::Json& right) {
        if (left.is_number() && right.is_number()) {
            const double a = left.get<double>();
            const double b = right.get<double>();
            const double scale = std::max({1.0, std::abs(a), std::abs(b)});
            return std::abs(a - b) <= 1e-6 * scale;
        }
        if (left.type() != right.type()) return false;
        if (left.is_array()) {
            if (left.size() != right.size()) return false;
            for (std::size_t i = 0; i < left.size(); ++i) {
                if (!equivalentJson(left[i], right[i])) return false;
            }
            return true;
        }
        if (left.is_object()) {
            if (left.size() != right.size()) return false;
            for (const auto& [key, value] : left.items()) {
                const auto it = right.find(key);
                if (it == right.end() || !equivalentJson(value, *it)) return false;
            }
            return true;
        }
        return left == right;
    }
};

#endif
