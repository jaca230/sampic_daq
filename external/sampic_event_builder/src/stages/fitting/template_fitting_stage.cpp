#include "sampic_event_builder/stages/fitting/template_fitting_stage.h"

#include <limits>

namespace sampic::event_builder {
void TemplateFittingStage::fit(PipelineEvent& event) const {
    event.waveforms.clear(); event.waveforms.reserve(event.source.hits.size());
    for(const auto& hit:event.source.hits) {
        auto fit=fitter_.fit(hit.waveform); PipelineWaveform item;
        auto& p=item.product; p.crate=hit.crate; p.board=hit.board; p.channel=hit.channel;
        p.timestamp_ns=hit.timestamp_ns; p.sample_count=hit.waveform.size();
        p.fitter_id=fitter_.algorithm_id(); p.template_id=config_.template_id;
        p.baseline=fit.baseline; p.amplitude_q=fit.amplitude_q; p.shift_q=fit.shift_q;
        p.chi_squared=static_cast<float>(fit.chi_squared);
        p.degrees_of_freedom=fit.degrees_of_freedom;
        const double reduced = fit.degrees_of_freedom == 0
            ? std::numeric_limits<double>::infinity()
            : fit.chi_squared / fit.degrees_of_freedom;
        if (reduced <= config_.maximum_reduced_chi_squared) {
            p.flags|=ProcessingFlags::fitted;
            item.values=std::move(fit.residuals);
        } else {
            p.flags|=ProcessingFlags::fit_rejected;
            item.values.assign(hit.waveform.begin(),hit.waveform.end());
        }
        event.waveforms.push_back(std::move(item));
    }
}
} // namespace sampic::event_builder
