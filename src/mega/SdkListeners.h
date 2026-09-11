#pragma once

#include <megaapi.h>

#include <functional>
#include <utility>

namespace megasdk
{

// Adapts one MegaApi request to std::functions. Both run on the SDK's thread.
//
// Lifetime: `new` it at the call site and hand it to MegaApi; it deletes itself
// from onRequestFinish. The SDK never deletes listeners, and it always delivers
// onRequestFinish, even for requests aborted while ~MegaApi tears down.
// Do NOT call removeRequestListener on one: it does not unsubscribe but nulls the
// listener, so onRequestFinish never arrives and the object leaks.
class RequestListener final : public mega::MegaRequestListener
{
public:
    using Finish = std::function<void(mega::MegaRequest& request, mega::MegaError& error)>;
    using Update = std::function<void(mega::MegaRequest& request)>;

    explicit RequestListener(Finish onFinish, Update onUpdate = {})
        : mOnFinish(std::move(onFinish)), mOnUpdate(std::move(onUpdate))
    {}

    // The SDK sends updates for TYPE_FETCH_NODES only.
    void onRequestUpdate(mega::MegaApi* /*api*/, mega::MegaRequest* request) override
    {
        if (mOnUpdate)
        {
            mOnUpdate(*request);
        }
    }

    void onRequestFinish(mega::MegaApi* /*api*/,
                         mega::MegaRequest* request,
                         mega::MegaError* error) override
    {
        mOnFinish(*request, *error);
        delete this;
    }

private:
    Finish mOnFinish;
    Update mOnUpdate;
};

} // namespace megasdk
