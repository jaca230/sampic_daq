"""Complete simulated raw-stream configuration for one SAMPIC crate."""

from argparse import ArgumentParser, Namespace
from typing import Sequence

from profiles.profile_definition import OdbProfile, OdbWrite, settings_root


class StreamingSimulatorProfile(OdbProfile):
    name = "streaming_simulator"
    description = "Generate noisy ordered-raw waveforms for MIDAS event-builder tests."

    def configure_parser(self, parser: ArgumentParser) -> None:
        parser.add_argument("--events-per-cycle", type=int, default=1)
        parser.add_argument("--hits-per-event", type=int, default=16)
        parser.add_argument("--waveform-length", type=int, default=64)
        parser.add_argument("--read-time-us", type=int, default=1000)
        parser.add_argument("--inter-event-gap-ns", type=float, default=1000.0)
        parser.add_argument("--noise-rms-adc", type=float, default=3.0)
        parser.add_argument("--amplitude-jitter", type=float, default=0.05)
        parser.add_argument("--trigger-jitter-samples", type=float, default=1.25)
        parser.add_argument("--punch-through-fraction", type=float, default=0.75)
        parser.add_argument("--random-seed", type=int, default=1)

    def build_writes(self, arguments: Namespace) -> Sequence[OdbWrite]:
        if arguments.events_per_cycle <= 0 or arguments.hits_per_event <= 0:
            raise ValueError("event and hit counts must be positive")
        if not 1 <= arguments.waveform_length <= 64:
            raise ValueError("waveform length must be in [1, 64]")
        if arguments.read_time_us < 0 or arguments.inter_event_gap_ns < 0:
            raise ValueError("timing values must be non-negative")
        if arguments.noise_rms_adc < 0 or arguments.amplitude_jitter < 0:
            raise ValueError("noise and jitter must be non-negative")

        root = settings_root(arguments.frontend_index)
        controller = f"{root}/Sampic Controller"
        collector = f"{root}/Sampic Event Collector"
        frontend = f"{root}/Frontend Event Collector"
        simulator = f"{collector}/modes/simulator"
        raw = f"{frontend}/modes/vendor_raw_passthrough"
        # Common may be created before mfe registers equipment. Supply the
        # operational fields too: generic MIDAS record defaults are disabled.
        common = f"/Equipment/SAMPIC {arguments.frontend_index:02d}/Common"
        return [
            OdbWrite(f"{common}/Event ID", 1, "Raw SAMPIC event identifier.", "WORD"),
            OdbWrite(f"{common}/Type", 256, "MIDAS EQ_USER equipment."),
            OdbWrite(f"{common}/Format", "MIDAS", "Banked MIDAS events."),
            OdbWrite(f"{common}/Enabled", True, "Enable the simulated source."),
            OdbWrite(f"{common}/Read on", 1, "Read while running."),
            OdbWrite(f"{common}/Period", 100, "Equipment scheduler period in ms."),
            OdbWrite(f"/Equipment/SAMPIC {arguments.frontend_index:02d}/Common/Buffer",
                     f"SBUF{arguments.frontend_index:02d}",
                     "Send raw crate events only to the dedicated builder input buffer."),
            OdbWrite(f"{controller}/init_mode", "simulator",
                     "Bypass hardware discovery."),
            OdbWrite(f"{controller}/apply_mode", "simulator",
                     "Bypass hardware configuration."),
            OdbWrite(f"{controller}/init_modes/simulator/simulated_febs", 1,
                     "Expose one simulated FEB."),
            OdbWrite(f"{collector}/mode", "simulator",
                     "Generate vendor-compatible EventStruct objects."),
            OdbWrite(f"{collector}/buffer_size", 2048,
                     "Buffer generated vendor events."),
            OdbWrite(f"{collector}/sleep_time_us", 0,
                     "The simulator controls its own rate."),
            OdbWrite(f"{simulator}/events_per_cycle", arguments.events_per_cycle,
                     "Events generated per simulator invocation."),
            OdbWrite(f"{simulator}/hits_per_event", arguments.hits_per_event,
                     "Raw hits in each vendor event."),
            OdbWrite(f"{simulator}/waveform_length", arguments.waveform_length,
                     "ADC samples in each hit."),
            OdbWrite(f"{simulator}/simulate_read_time_us", arguments.read_time_us,
                     "Delay per simulator cycle."),
            OdbWrite(f"{simulator}/hit_time_step_ns", 5.0,
                     "Time separation among hits in one vendor event."),
            OdbWrite(f"{simulator}/inter_event_gap_ns",
                     arguments.inter_event_gap_ns,
                     "Shared-clock gap between generated events."),
            OdbWrite(f"{simulator}/start_timestamp_ns", 1_000_000.0,
                     "Shared timestamp origin for every crate."),
            OdbWrite(f"{simulator}/baseline_level", 0.05,
                     "Waveform baseline as ADC full-scale fraction."),
            OdbWrite(f"{simulator}/signal_amplitude", 0.8,
                     "Pulse amplitude as ADC full-scale fraction."),
            OdbWrite(f"{simulator}/noise_rms_adc", arguments.noise_rms_adc,
                     "Gaussian electronics-noise RMS in ADC counts."),
            OdbWrite(f"{simulator}/amplitude_jitter_fraction",
                     arguments.amplitude_jitter,
                     "Gaussian fractional pulse-amplitude spread."),
            OdbWrite(f"{simulator}/trigger_jitter_samples",
                     arguments.trigger_jitter_samples,
                     "Gaussian trigger-time jitter in waveform samples."),
            OdbWrite(f"{simulator}/punch_through_fraction",
                     arguments.punch_through_fraction,
                     "Fraction of low-amplitude all-plane positron tracks."),
            OdbWrite(f"{simulator}/punch_amplitude_fraction", 0.20,
                     "Pulse amplitude relative to retained physics events."),
            OdbWrite(f"{simulator}/random_seed",
                     arguments.random_seed + arguments.frontend_index,
                     "Independent deterministic random stream per crate."),
            OdbWrite(f"{frontend}/mode", "vendor_raw_passthrough",
                     "Preserve vendor boundaries and publish ordered raw ADC."),
            OdbWrite(f"{frontend}/buffer_size", 2048,
                     "Buffer MIDAS-ready events."),
            OdbWrite(f"{frontend}/sleep_time_us", 0,
                     "Do not add a frontend packaging delay."),
            OdbWrite(f"{raw}/raw_data_bank_enabled", True,
                     "Write SRnn ordered-raw banks."),
            OdbWrite(f"{raw}/raw_data_bank_prefix", "SR",
                     "Use the event-builder raw-bank contract."),
            OdbWrite(f"{raw}/acquisition_timing_bank_prefix", "SA",
                     "Use the event-builder acquisition timing contract."),
            OdbWrite(f"{raw}/acquisition_timing_bank_enabled", True,
                     "Emit one SA acquisition timing record per vendor event."),
            OdbWrite(f"{raw}/trigger_bank_enabled", False,
                     "Exclude empty trigger payload from rate tests."),
        ]


PROFILE = StreamingSimulatorProfile()
