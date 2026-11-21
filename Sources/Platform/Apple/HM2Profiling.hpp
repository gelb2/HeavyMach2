#pragma once
#import <os/signpost.h>

namespace hm2::apple {
inline os_log_t profilingLog(){static os_log_t value=os_log_create("com.gelb2.heavymach2","Runtime");return value;}
// Signpost recording is controlled by Instruments. No per-frame file, JSON,
// console output, or game-clock/RNG change is introduced.
class ProfileSpan {
    os_log_t log_=profilingLog();os_signpost_id_t identifier_=OS_SIGNPOST_ID_INVALID;
public:
    enum class Phase{tick,frameBuild,present};
private:
    Phase phase_;
public:
    explicit ProfileSpan(Phase phase):phase_(phase){
        if(!os_signpost_enabled(log_))return;identifier_=os_signpost_id_generate(log_);
        switch(phase_){
            case Phase::tick:os_signpost_interval_begin(log_,identifier_,"GameTick");break;
            case Phase::frameBuild:os_signpost_interval_begin(log_,identifier_,"FrameBuild");break;
            case Phase::present:os_signpost_interval_begin(log_,identifier_,"GLSubmitAndPresent");break;
        }
    }
    ~ProfileSpan(){if(identifier_==OS_SIGNPOST_ID_INVALID)return;switch(phase_){
        case Phase::tick:os_signpost_interval_end(log_,identifier_,"GameTick");break;
        case Phase::frameBuild:os_signpost_interval_end(log_,identifier_,"FrameBuild");break;
        case Phase::present:os_signpost_interval_end(log_,identifier_,"GLSubmitAndPresent");break;
    }}
};
}
