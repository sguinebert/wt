/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements. See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership. The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License. You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied. See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef CUEHTTP_ROUTER_HPP_
#define CUEHTTP_ROUTER_HPP_

#include <functional>
#include <unordered_map>

#include "context.hpp"
//#include "cuehttp/deps/fmt/fmt.h"
#include <Wt/fmt/format.h>
#include "detail/common.hpp"
#include "detail/noncopyable.hpp"

#include <boost/url.hpp>

namespace urls = boost::urls;

namespace Wt {
namespace http {

using namespace std::string_view_literals;

class router final : safe_noncopyable {
public:
    router() noexcept = default;

    //     /* The matching tree */
    //     struct Node {
    //         std::string name_;
    //         std::vector<std::string_view> segments_;
    //         std::vector<std::unique_ptr<Node>> children;
    //         //std::vector<uint32_t> handlers;
    //         bool isHighPriority = false;
    //         std::function<awaitable<void>(context&)> handler_;

    //         Node* match(url_view& u) const
    //         {
    //             Node *target = nullptr;
    //             auto segs = u.encoded_segments();
    //             auto ccc = segs.size();
    //             auto ccc2 = segments_.size();

    //             if(segs.size() < segments_.size())
    //                 return nullptr;
    //             bool mc = std::equal(segments_.begin(),
    //                                  segments_.end(),
    //                                  segs.begin());
    //             if(mc) {
    //                 target = (Node*)this;
    //                 for(auto& child : children) {
    //                     if(auto mcc = child->match(u); mcc != nullptr)
    //                         return mcc;
    //                 }
    //             }
    //             return target;
    //         }

    //         Node(std::string name) : name_(name) {
    //             urls::url_view uv(name_);
    //             for(auto v : uv.encoded_segments())
    //                 segments_.push_back(v);
    //         }
    //     } root_ {prefix_};

    template <typename _Prefix, typename = std::enable_if_t<!std::is_same_v<std::decay_t<_Prefix>, router>>>
    explicit router(_Prefix&& prefix) noexcept : prefix_{std::forward<_Prefix>(prefix)} {}

    std::function<awaitable<void>(context&)> routes() const noexcept { return make_routes(); }

    template <typename _Prefix>
    router& prefix(_Prefix&& prefix) {
        prefix_ = std::forward<_Prefix>(prefix);
        return *this;
    }

    template <typename... _Args>
    router& del(std::string_view path, _Args&&... args) {
        register_impl({"DEL"sv}, path, std::forward<_Args>(args)...);
        return *this;
    }

    template <typename... _Args>
    router& get(std::string_view path, _Args&&... args) {
        register_impl({"GET"sv}, path, std::forward<_Args>(args)...);
        return *this;
    }

    template <typename... _Args>
    router& head(std::string_view path, _Args&&... args) {
        register_impl({"HEAD"sv}, path, std::forward<_Args>(args)...);
        return *this;
    }

    template <typename... _Args>
    router& post(std::string_view path, _Args&&... args) {
        register_impl({"POST"sv}, path, std::forward<_Args>(args)...);
        return *this;
    }

    template <typename... _Args>
    router& put(std::string_view path, _Args&&... args) {
        register_impl({"PUT"sv}, path, std::forward<_Args>(args)...);
        return *this;
    }

    template <typename... _Args>
    router& all(std::string_view path, _Args&&... args) {
        static std::vector<std::string_view> methods{"DEL"sv, "GET"sv, "HEAD"sv, "POST"sv, "PUT"sv};
        //for (const auto& method : methods) {
        register_impl(methods, path, std::forward<_Args>(args)...);
        //}
        return *this;
    }

    template <typename... _Args>
    router& redirect(_Args&&... args) {
        redirect_impl(std::forward<_Args>(args)...);
        return *this;
    }

    operator auto() const noexcept { return make_routes(); }

private:
    template <typename _Func, typename = std::enable_if_t<!std::is_member_function_pointer_v<_Func>>, typename... _Args>
    void register_impl(std::vector<std::string_view> method, std::string_view path, _Func&& func, _Args&&... args) {
        std::vector<std::function<void(context&, std::function<void()>)>> handlers;
        register_multiple(handlers, std::forward<_Func>(func), std::forward<_Args>(args)...);
        compose(method, path, std::move(handlers));
    }

    template <typename _Ty, typename _Func, typename _Self, typename = std::enable_if_t<std::is_same_v<_Ty*, _Self>>,
             typename... _Args>
    void register_impl(std::vector<std::string_view> method, std::string_view path,
                       _Func (_Ty::*func)(context&, std::function<void()>), _Self self, _Args&&... args) {
        std::vector<std::function<void(context&, std::function<void()>)>> handlers;
        register_multiple(handlers, func, self, std::forward<_Args>(args)...);
        compose(method, path, std::move(handlers));
    }

    template <typename _Ty, typename _Func, typename... _Args>
    void register_impl(std::vector<std::string_view> method, std::string_view path,
                       _Func (_Ty::*func)(context&, std::function<void()>), _Args&&... args) {
        std::vector<std::function<void(context&, std::function<void()>)>> handlers;
        register_multiple(handlers, func, std::forward<_Args>(args)...);
        compose(method, path, std::move(handlers));
    }

    template <typename _Ty, typename _Func, typename _Self, typename = std::enable_if_t<std::is_same_v<_Ty*, _Self>>,
             typename... _Args>
    void register_impl(std::vector<std::string_view> method, std::string_view path, _Func (_Ty::*func)(context&), _Self self,
                       _Args&&... args) {
        std::vector<std::function<void(context&, std::function<void()>)>> handlers;
        register_multiple(handlers, func, self, std::forward<_Args>(args)...);
        compose(method, path, std::move(handlers));
    }

    template <typename _Ty, typename _Func, typename... _Args>
    void register_impl(std::vector<std::string_view> method, std::string_view path, _Func (_Ty::*func)(context&), _Args&&... args) {
        std::vector<std::function<void(context&, std::function<void()>)>> handlers;
        register_multiple(handlers, func, std::forward<_Args>(args)...);
        compose(method, path, std::move(handlers));
    }

    template <typename _Func, typename... _Args>
    std::enable_if_t<detail::is_middleware_v<_Func>, std::true_type> register_multiple(
        std::vector<std::function<void(context&, std::function<void()>)>>& handlers, _Func&& func, _Args&&... args) {
        handlers.emplace_back(std::forward<_Func>(func));
        register_multiple(handlers, std::forward<_Args>(args)...);
        return std::true_type{};
    }

    template <typename _Func, typename... _Args>
    std::enable_if_t<detail::is_middleware_v<_Func>, std::true_type> register_multiple(
        std::vector<std::function<void(context&, std::function<void()>)>>& handlers, _Func&& func) {
        handlers.emplace_back(std::forward<_Func>(func));
        return std::true_type{};
    }

    template <typename _Func, typename... _Args>
    std::enable_if_t<detail::is_middleware_without_next_v<_Func>, std::false_type> register_multiple(
        std::vector<std::function<void(context&, std::function<void()>)>>& handlers, _Func&& func, _Args&&... args) {
        handlers.emplace_back([func = std::forward<_Func>(func)](context& ctx, std::function<void()> next) {
            func(ctx);
            next();
        });
        register_multiple(handlers, std::forward<_Args>(args)...);
        return std::false_type{};
    }

    template <typename _Func, typename... _Args>
    std::enable_if_t<detail::is_middleware_without_next_v<_Func>, std::false_type> register_multiple(
        std::vector<std::function<void(context&, std::function<void()>)>>& handlers, _Func&& func) {
        handlers.emplace_back([func = std::forward<_Func>(func)](context& ctx, std::function<void()> next) {
            func(ctx);
            next();
        });
        return std::false_type{};
    }

    template <typename _Ty, typename _Func, typename _Self, typename = std::enable_if_t<std::is_same_v<_Ty*, _Self>>,
             typename... _Args>
    void register_multiple(std::vector<std::function<void(context&, std::function<void()>)>>& handlers,
                           _Func (_Ty::*func)(context&, std::function<void()>), _Self self, _Args&&... args) {
        handlers.emplace_back([func, self](context& ctx, std::function<void()> next) {
            if (self) {
                (self->*func)(ctx, std::move(next));
            }
        });
        register_multiple(handlers, std::forward<_Args>(args)...);
    }

    template <typename _Ty, typename _Func, typename... _Args>
    void register_multiple(std::vector<std::function<void(context&, std::function<void()>)>>& handlers,
                           _Func (_Ty::*func)(context&, std::function<void()>), _Args&&... args) {
        handlers.emplace_back([func](context& ctx, std::function<void()> next) { (_Ty{}.*func)(ctx, std::move(next)); });
        register_multiple(handlers, std::forward<_Args>(args)...);
    }

    template <typename _Ty, typename _Func, typename _Self, typename = std::enable_if_t<std::is_same_v<_Ty*, _Self>>,
             typename... _Args>
    void register_multiple(std::vector<std::function<void(context&, std::function<void()>)>>& handlers,
                           _Func (_Ty::*func)(context&), _Self self, _Args&&... args) {
        handlers.emplace_back([func, self](context& ctx, std::function<void()> next) {
            if (self) {
                (self->*func)(ctx);
            }
            next();
        });
        register_multiple(handlers, std::forward<_Args>(args)...);
    }

    template <typename _Ty, typename _Func, typename... _Args>
    void register_multiple(std::vector<std::function<void(context&, std::function<void()>)>>& handlers,
                           _Func (_Ty::*func)(context&), _Args&&... args) {
        handlers.emplace_back([func](context& ctx, std::function<void()> next) {
            (_Ty{}.*func)(ctx);
            next();
        });
        register_multiple(handlers, std::forward<_Args>(args)...);
    }

    template <typename _Ty, typename _Func, typename _Self, typename = std::enable_if_t<std::is_same_v<_Ty*, _Self>>>
    void register_multiple(std::vector<std::function<void(context&, std::function<void()>)>>& handlers,
                           _Func (_Ty::*func)(context&, std::function<void()>), _Self self) {
        handlers.emplace_back([func, self](context& ctx, std::function<void()> next) {
            if (self) {
                (self->*func)(ctx, std::move(next));
            }
        });
    }

    template <typename _Ty, typename _Func>
    void register_multiple(std::vector<std::function<void(context&, std::function<void()>)>>& handlers,
                           _Func (_Ty::*func)(context&, std::function<void()>)) {
        handlers.emplace_back([func](context& ctx, std::function<void()> next) { (_Ty{}.*func)(ctx, std::move(next)); });
    }

    template <typename _Ty, typename _Func, typename _Self, typename = std::enable_if_t<std::is_same_v<_Ty*, _Self>>>
    void register_multiple(std::vector<std::function<void(context&, std::function<void()>)>>& handlers,
                           _Func (_Ty::*func)(context&), _Self self) {
        handlers.emplace_back([func, self](context& ctx, std::function<void()> next) {
            if (self) {
                (self->*func)(ctx);
            }
        });
    }

    template <typename _Ty, typename _Func>
    void register_multiple(std::vector<std::function<void(context&, std::function<void()>)>>& handlers,
                           _Func (_Ty::*func)(context&)) {
        handlers.emplace_back([func](context& ctx, std::function<void()> next) { (_Ty{}.*func)(ctx); });
    }

    template <typename _Func, typename = std::enable_if_t<!std::is_member_function_pointer_v<_Func>>>
    std::enable_if_t<detail::is_middleware_v<_Func>, std::true_type> register_impl(std::vector<std::string_view> method,
                                                                                   std::string_view path, _Func&& func) {
        register_with_next(method, path, std::forward<_Func>(func));
        return std::true_type{};
    }

    template <typename _Ty, typename _Func, typename _Self, typename = std::enable_if_t<std::is_same_v<_Ty*, _Self>>>
    void register_impl(std::vector<std::string_view> method, std::string_view path,
                       _Func (_Ty::*func)(context&, std::function<void()>), _Self self) {
        register_with_next(method, path, func, self);
    }

    template <typename _Ty, typename _Func>
    void register_impl(std::vector<std::string_view> method, std::string_view path,
                       _Func (_Ty::*func)(context&, std::function<void()>)) {
        register_with_next(method, path, func, (_Ty*)nullptr);
    }

    template <typename _Func, typename = std::enable_if_t<!std::is_member_function_pointer_v<_Func>>>
    std::enable_if_t<detail::is_awaitable_lambda_v<_Func>, std::false_type> register_impl(std::vector<std::string_view> method,
                                                                                          std::string_view path,
                                                                                          _Func&& func) {
        register_without_next(method, path, std::forward<_Func>(func));
        return std::false_type{};
    }

    template <typename _Func, typename = std::enable_if_t<!std::is_member_function_pointer_v<_Func>>>
    std::enable_if_t<detail::is_middleware_classic_without_next_v<_Func> && !detail::is_awaitable_lambda_v<_Func>, std::true_type>
    register_impl(std::vector<std::string_view> method,
                  std::string_view path,
                  _Func&& func) {
        register_classic_without_next(method, path, std::forward<_Func>(func));
        return std::true_type{};
    }

    template <typename _Ty, typename _Func, typename _Self, typename = std::enable_if_t<std::is_same_v<_Ty*, _Self>>>
    void register_impl(std::vector<std::string_view> method, std::string_view path, _Func (_Ty::*func)(context&), _Self self) {
        register_without_next(method, path, func, self);
    }

    template <typename _Ty, typename _Func>
    void register_impl(std::vector<std::string_view> method, std::string_view path, _Func (_Ty::*func)(context&)) {
        register_without_next(method, path, func, (_Ty*)nullptr);
    }

    //  template <typename _Func>
    //  void register_with_next(std::string_view method, std::string_view path, _Func&& func) {
    //    handlers_.emplace(fmt::format("{}+{}{}", method, prefix_, path), [func = std::forward<_Func>(func)](context& ctx) {
    //      const auto next = []() {};
    //      func(ctx, std::move(next));
    //    });
    //  }

    /* with next()
   *
   */
    template <typename _Func>
    void register_with_next(std::vector<std::string_view> method, std::string_view path, _Func&& func) {
        add(method, path, [func = std::forward<_Func>(func)](context& ctx) -> awaitable<void> {
            const auto next = []() {};
            co_await func(ctx, std::move(next));
        });

        //    handlers_.emplace(fmt::format("{}+{}{}", method, prefix_, path), [func = std::forward<_Func>(func)](context& ctx) -> awaitable<void> {
        //      const auto next = []() {};
        //      co_await func(ctx, std::move(next));
        //    });
    }

    template <typename _Ty, typename _Func, typename _Self>
    void register_with_next(std::vector<std::string_view> method, std::string_view path, _Func _Ty::*func, _Self self) {
        add(method, path, [func, self](context& ctx) -> awaitable<void> {
            const auto next = []() {};
            if (self) {
                (self->*func)(ctx, std::move(next));
            } else {
                (_Ty{}.*func)(ctx, std::move(next));
            }
            co_return;
        });
        //    handlers_.emplace(fmt::format("{}+{}{}", method, prefix_, path), [func, self](context& ctx) -> awaitable<void> {
        //      const auto next = []() {};
        //      if (self) {
        //        (self->*func)(ctx, std::move(next));
        //      } else {
        //        (_Ty{}.*func)(ctx, std::move(next));
        //      }
        //    });
    }

    /* without next()
   *
   */
    template <typename _Func>
    void register_without_next(std::vector<std::string_view> method, std::string_view path, _Func&& func) {
        add(method, path, std::forward<_Func>(func));
        //handlers_.emplace(fmt::format("{}+{}{}", method, prefix_, path), std::forward<_Func>(func));
    }

    template <typename _Ty, typename _Func, typename _Self>
    void register_without_next(std::vector<std::string_view> method, std::string_view path, _Func _Ty::*func, _Self self) {

        add(method, path, [func, self](context& ctx) -> awaitable<void> {
            if (self) {
                (self->*func)(ctx);
            } else {
                (_Ty{}.*func)(ctx);
            }
            co_return;
        });

        //    handlers_.emplace(fmt::format("{}+{}{}", method, prefix_, path), [func, self](context& ctx) -> awaitable<void> {
        //      if (self) {
        //        (self->*func)(ctx);
        //      } else {
        //        (_Ty{}.*func)(ctx);
        //      }
        //    });
    }

    /* classic function (not a coroutine) without next()
   *
   */
    template <typename _Func>
    void register_classic_without_next(std::vector<std::string_view> method, std::string_view path, _Func&& func) {
        add(method, path, [func = std::move(func)](context& ctx) -> awaitable<void> {
            func(ctx);
            co_return;
        });
        //      handlers_.emplace(fmt::format("{}+{}{}", method, prefix_, path), [func = std::move(func)](context& ctx) -> awaitable<void> {
        //        func(ctx);
        //        co_return;
        //      });
    }

    /* redirect
   *
   */
    template <typename _Dest>
    void redirect_impl(std::string_view path, _Dest&& destination) {
        redirect_impl(path, std::forward<_Dest>(destination), 301);
    }

    template <typename _Dest>
    void redirect_impl(std::string_view path, _Dest&& destination, unsigned status) {
        all(path, [destination = std::forward<_Dest>(destination), status](context& ctx) {
            ctx.redirect(std::move(destination));
            ctx.status(status);
        });
    }

    void compose(std::vector<std::string_view> method, std::string_view path,
                 std::vector<std::function<void(context&, std::function<void()>)>>&& handlers) {
        const auto handler = [handlers = std::move(handlers)](context& ctx) -> awaitable<void> {
            if (handlers.empty()) {
                co_return;
            }

            if (handlers.size() == 1) {
                handlers[0](ctx, []() {});
            } else {
                std::size_t index{0};
                std::function<void()> next;
                next = [&handlers, &next, &index, &ctx]() {
                    if (++index == handlers.size()) {
                        return;
                    }
                    handlers[index](ctx, next);
                };

                handlers[0](ctx, next);
            }
        };
        add(method, path, std::move(handler));
        //handlers_.emplace(fmt::format("{}+{}{}", method, prefix_, path), std::move(handler));
    }

    std::function<awaitable<void>(context&)> make_routes() const noexcept {
        return [this](context& ctx) -> awaitable<void>  {
            if (ctx.status() != 404) {
                co_return;
            }

            auto method = ctx.method();
            /* Begin by finding the method node */
            for (auto &p : root.children) {
                if (p->name == method) {
                    /* Then route the url */
                    auto urlv = ctx.urlv();
                    //boost::url_view cc { ctx.url() };
                    auto ssv = urlv.encoded_segments();
                    std::vector<std::string_view> segments { ssv.begin(), ssv.end() };
                    auto target = getHandlers(p.get(), segments, 0);
                    if(target && !target->handlers.empty())
                    {
                        for (uint32_t handler : target->handlers) {
                            co_await handlers[handler & HANDLER_MASK](ctx);
                        }
                        co_return;
                    }
                    else {
                        /* try file resource if docroot defined */
//                        auto uv = ctx.urlv();
//                        for(auto vf : uv.encoded_segments())
//                            std::cerr << " data uv ;:" << vf << std::endl;

                        std::cerr << "ERROR router url : " << ctx.url() << std::endl;
                        ctx.flush();
                    }
                }
            }

            co_return;

            //      const auto it = handlers_.find(fmt::format("{}+{}{}", ctx.method(), prefix_, ctx.path()));
            //      if (it != handlers_.end()) {
            //        co_await it->second(ctx);
            //      }
            //      co_return;
        };
    }


    std::string prefix_ {""};
    //std::unordered_map<std::string, std::function<void(context&)>> handlers_;
    std::unordered_map<std::string, std::function<awaitable<void>(context&)>> handlers_;

    /* These are public for now */
    std::vector<std::string> upperCasedMethods = {"GET", "POST", "HEAD", "PUT", "DELETE", "CONNECT", "OPTIONS", "TRACE", "PATCH"};
    static const uint32_t HIGH_PRIORITY = 0xd0000000, MEDIUM_PRIORITY = 0xe0000000, LOW_PRIORITY = 0xf0000000;

private:
    //USERDATA userData;
    static const unsigned int MAX_URL_SEGMENTS = 100;

    /* Handler ids are 32-bit */
    static const uint32_t HANDLER_MASK = 0x0fffffff;

    /* Methods and their respective priority */
    std::map<std::string, int> priority;

    /* List of handlers */
    std::vector<std::function<awaitable<void>(context&)>> handlers;

    /* Current URL cache */
    std::string_view currentUrl;
    std::string_view urlSegmentVector[MAX_URL_SEGMENTS];
    int urlSegmentTop;

    /* The matching tree */
    struct Node {
        std::string name;
        std::vector<std::unique_ptr<Node>> children;
        std::vector<uint32_t> handlers;
        bool isHighPriority;

        Node(std::string_view name) : name(name) {}
    } root = {"rootNode"};

    /* Sort wildcards after alphanum */
    int lexicalOrder(std::string &name) {
        if (!name.length()) {
            return 2;
        }
        if (name[0] == ':') {
            return 1;
        }
        if (name[0] == '*') {
            return 0;
        }
        return 2;
    }

    /* Advance from parent to child, adding child if necessary */
    Node *getNode(Node *parent, std::string_view child, bool isHighPriority) {
        for (std::unique_ptr<Node> &node : parent->children) {
            if (node->name == child && node->isHighPriority == isHighPriority) {
                return node.get();
            }
        }

        /* Insert sorted, but keep order if parent is root (we sort methods by priority elsewhere) */
        std::unique_ptr<Node> newNode(new Node(child));
        newNode->isHighPriority = isHighPriority;
        return parent->children.emplace(std::upper_bound(parent->children.begin(), parent->children.end(), newNode, [parent, this](auto &a, auto &b) {

                                            if (a->isHighPriority != b->isHighPriority) {
                                                return a->isHighPriority;
                                            }

                                            return b->name.length() && (parent != &root) && (lexicalOrder(b->name) < lexicalOrder(a->name));
                                        }), std::move(newNode))->get();
    }

    /* Basically a pre-allocated stack */
    struct RouteParameters {
        friend class router;
    private:
        std::string_view params[MAX_URL_SEGMENTS];
        int paramsTop;

        void reset() {
            paramsTop = -1;
        }

        void push(std::string_view param) {
            /* We check these bounds indirectly via the urlSegments limit */
            params[++paramsTop] = param;
        }

        void pop() {
            /* Same here, we cannot pop outside */
            paramsTop--;
        }
    } mutable routeParameters;

    /* Set URL for router. Will reset any URL cache */
    inline void setUrl(std::string_view url) {

        /* Todo: URL may also start with "http://domain/" or "*", not only "/" */

        /* We expect to stand on a slash */
        currentUrl = url;
        urlSegmentTop = -1;
    }

    /* Lazily parse or read from cache */
    inline std::pair<std::string_view, bool> getUrlSegment(int urlSegment) {
        if (urlSegment > urlSegmentTop) {
            /* Signal as STOP when we have no more URL or stack space */
            if (!currentUrl.length() || urlSegment > 99) {
                return {{}, true};
            }

            /* We always stand on a slash here, so step over it */
            currentUrl.remove_prefix(1);

            auto segmentLength = currentUrl.find('/');
            if (segmentLength == std::string::npos) {
                segmentLength = currentUrl.length();

                /* Push to url segment vector */
                urlSegmentVector[urlSegment] = currentUrl.substr(0, segmentLength);
                urlSegmentTop++;

                /* Update currentUrl */
                currentUrl = currentUrl.substr(segmentLength);
            } else {
                /* Push to url segment vector */
                urlSegmentVector[urlSegment] = currentUrl.substr(0, segmentLength);
                urlSegmentTop++;

                /* Update currentUrl */
                currentUrl = currentUrl.substr(segmentLength);
            }
        }
        /* In any case we return it */
        return {urlSegmentVector[urlSegment], false};
    }

    /* Executes as many handlers it can */
    awaitable<bool> executeHandlers(Node *parent, context& ctx, int urlSegment) {

        auto [segment, isStop] = getUrlSegment(urlSegment);

        /* If we are on STOP, return where we may stand */
        if (isStop) {
            /* We have reached accross the entire URL with no stoppage, execute */
            for (uint32_t handler : parent->handlers) {
                co_await handlers[handler & HANDLER_MASK](ctx);
                //if () {
                co_return true;
                //}
            }
            /* We reached the end, so go back */
            co_return false;
        }

        for (auto &p : parent->children) {
            if (p->name.length() && p->name[0] == '*') {
                /* Wildcard match (can be seen as a shortcut) */
                for (uint32_t handler : p->handlers) {
                    if (co_await handlers[handler & HANDLER_MASK](ctx); true) {
                        co_return true;
                    }
                }
            } else if (p->name.length() && p->name[0] == ':' && segment.length()) {
                /* Parameter match */
                routeParameters.push(segment);
                if (co_await executeHandlers(p.get(), ctx, urlSegment + 1)) {
                    co_return true;
                }
                routeParameters.pop();
            } else if (p->name == segment) {
                /* Static match */
                if (co_await executeHandlers(p.get(), ctx, urlSegment + 1)) {
                    co_return true;
                }
            }
        }
        co_return false;
    }

    Node* getHandlers(Node *parent, std::vector<std::string_view>& segments, unsigned urlSegment) const {
        /* If we are on STOP, return where we may stand */
        if (segments.size() == urlSegment) {
            /* We have reached accross the entire URL with no stoppage, execute */
            return parent;
        }

        auto segment = segments[urlSegment];

        for (auto &p : parent->children)
        {
            if (p->name.length() && p->name[0] == '*')
            {
                /* Wildcard match (can be seen as a shortcut) */
                return p.get();
            }
            else if (p->name.length() && p->name[0] == ':' && segment.length())
            {
                /* Parameter match */
                routeParameters.push(segment);
                if (auto subnode = getHandlers(p.get(), segments, urlSegment + 1))
                {
                    return subnode;
                }
                routeParameters.pop();
            }
            else if (p->name == segment)
            {
                /* Static match */
                if (auto subnode = getHandlers(p.get(), segments, urlSegment + 1))
                {
                    return subnode;
                }
            }
        }
        return nullptr;
    }

    /* Scans for one matching handler, returning the handler and its priority or UINT32_MAX for not found */
    uint32_t findHandler(std::string_view method, std::string_view pattern, uint32_t priority) {
        for (std::unique_ptr<Node> &node : root.children) {
            if (method == node->name) {
                setUrl(pattern);
                Node *n = node.get();
                for (int i = 0; !getUrlSegment(i).second; i++) {
                    /* Go to next segment or quit */
                    auto segment = getUrlSegment(i).first;
                    Node *next = nullptr;
                    for (std::unique_ptr<Node> &child : n->children) {
                        if (child->name == segment && child->isHighPriority == (priority == HIGH_PRIORITY)) {
                            next = child.get();
                            break;
                        }
                    }
                    if (!next) {
                        return UINT32_MAX;
                    }
                    n = next;
                }
                /* Seek for a priority match in the found node */
                for (unsigned int i = 0; i < n->handlers.size(); i++) {
                    if ((n->handlers[i] & ~HANDLER_MASK) == priority) {
                        return n->handlers[i];
                    }
                }
                return UINT32_MAX;
            }
        }
        return UINT32_MAX;
    }

public:

    //  std::pair<int, std::string_view *> getParameters() {
    //    return {routeParameters.paramsTop, routeParameters.params};
    //  }

    //  USERDATA &getUserData() {
    //    return userData;
    //  }

    /* Fast path */
    //  bool route(std::string_view method, std::string_view url) {
    //    /* Reset url parsing cache */
    //    setUrl(url);
    //    routeParameters.reset();

    //    /* Begin by finding the method node */
    //    for (auto &p : root.children) {
    //        if (p->name == method) {
    //        /* Then route the url */
    //        return executeHandlers(p.get(), 0);
    //        }
    //    }

    //    /* We did not find any handler for this method and url */
    //    return false;
    //  }

    /* Adds the corresponding entires in matching tree and handler list */
    void add(std::vector<std::string_view> methods, std::string_view pattern, std::function<awaitable<void>(context&)> &&handler, uint32_t priority = MEDIUM_PRIORITY) {
        for (auto& method : methods) {
            /* Lookup method */
            Node *node = getNode(&root, method, false);
            /* Iterate over all segments */
            setUrl(pattern);
            for (int i = 0; !getUrlSegment(i).second; i++) {
                node = getNode(node, getUrlSegment(i).first, priority == HIGH_PRIORITY);
            }
            /* Insert handler in order sorted by priority (most significant 1 byte) */
            node->handlers.insert(std::upper_bound(node->handlers.begin(), node->handlers.end(), (uint32_t) (priority | handlers.size())), (uint32_t) (priority | handlers.size()));
        }

        /* Alloate this handler */
        handlers.emplace_back(std::move(handler));

        /* Assume can find this handler again */
        if (((handlers.size() - 1) | priority) != findHandler(methods[0], pattern, priority)) {
            std::cerr << "Error: Internal routing error" << std::endl;
            std::abort();
        }
    }

    bool cullNode(Node *parent, Node *node, uint32_t handler) {
        /* For all children */
        for (unsigned int i = 0; i < node->children.size(); ) {
            /* Optimization todo: only enter those with same isHighPrioirty */
            /* Enter child so we get depth first */
            if (!cullNode(node, node->children[i].get(), handler)) {
                /* Only increase if this node was not removed */
                i++;
            }
        }

        /* Cull this node (but skip the root node) */
        if (parent /*&& parent != &root*/) {
            /* Scan for equal (remove), greater (lower by 1) */
            for (auto it = node->handlers.begin(); it != node->handlers.end(); ) {
                if ((*it & HANDLER_MASK) > (handler & HANDLER_MASK)) {
                    *it = ((*it & HANDLER_MASK) - 1) | (*it & ~HANDLER_MASK);
                } else if (*it == handler) {
                    it = node->handlers.erase(it);
                    continue;
                }
                it++;
            }

            /* If we have no children and no handlers, remove us from the parent->children list */
            if (!node->handlers.size() && !node->children.size()) {
                parent->children.erase(std::find_if(parent->children.begin(), parent->children.end(), [node](const std::unique_ptr<Node> &a) {
                    return a.get() == node;
                }));
                /* Returning true means we removed node from parent */
                return true;
            }
        }

        return false;
    }

    /* Removes ALL routes with the same handler as can be found with the given parameters.
     * Removing a wildcard is done by removing ONE OF the methods the wildcard would match with.
     * Example: If wildcard includes POST, GET, PUT, you can remove ALL THREE by removing GET. */
    void remove(std::string method, std::string pattern, uint32_t priority) {
        uint32_t handler = findHandler(method, pattern, priority);
        if (handler == UINT32_MAX) {
            /* Not found or already removed, do nothing */
            return;
        }

        /* Cull the entire tree */
        /* For all nodes in depth first tree traveral;
         * if node contains handler - remove the handler -
         * if node holds no handlers after removal, remove the node and return */
        cullNode(nullptr, &root, handler);

        /* Now remove the actual handler */
        handlers.erase(handlers.begin() + (handler & HANDLER_MASK));
    }

};

}  // namespace http
}  // namespace cue

#endif  // CUEHTTP_ROUTER_HPP_
