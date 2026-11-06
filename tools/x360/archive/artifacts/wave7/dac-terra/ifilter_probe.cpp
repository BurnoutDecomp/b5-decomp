#include "rw/audio/core/DelayFilter.h"
#include "rw/audio/core/DelayLine.h"
void NativeDelayApply(rw::audio::core::DelayLine* line, rw::audio::core::DelayFilter* filter, rw::audio::core::SampleBuffer* src, rw::audio::core::SampleBuffer* dst, int count) { filter->mpApplyFunc = &rw::audio::core::DelayFilter::DelayFilterApplyFunc; line->mpFilter = filter; line->ApplyFilter(count, src, dst, rw::audio::core::IFilter::READ); }
