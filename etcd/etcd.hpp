#pragma once
#include <etcd/Client.hpp>
#include <etcd/KeepAlive.hpp>
#include <etcd/Response.hpp>
#include <etcd/Value.hpp>
#include <etcd/Watcher.hpp>
#include "logger.hpp"
// 服务注册类客户端,提供一个服务注册的接口registry
/*需要的参数：1.etcd服务器的地址 2.要注册的服务信息*/
class Registry
{
public:
  using ptr=std::shared_ptr<Registry>;
  // 实例化客户端对象，连接服务器，获取租约id
  Registry(const std::string &host)
      : _client(std::make_shared<etcd::Client>(host)),
        _keep_alive(_client->leasekeepalive(3).get()),
        _lease_id(_keep_alive->Lease()) {}
  // 注册服务信息
  bool registry(const std::string &key, const std::string &value)
  {
    // 向etcd服务器添加一个键值对数据
    auto resp = _client->put(key, value, _lease_id).get(); // 该数据就会按照该租约形式续租
    if (resp.is_ok() == false)
    {
      DEBUG_LOG("添加数据失败:{}", resp.error_message());
      return false;
    }
    return true;
  }

private:
  std::shared_ptr<etcd::Client> _client;        // 客户端对象
  std::shared_ptr<etcd::KeepAlive> _keep_alive; // 保活对象
  uint64_t _lease_id;                           // 租约id
};
// 服务发现类客户端
/*要贴合项目，当有服务(上线)添加时要干什么由上层业务决定，定义一个回调函数，由用户传入，执行该函数操作
当有服务下线(被删除)时，定义一个回调函数，由用户传入，执行该函数操作*/
class Discovery
{
  public:
  using ptr=std::shared_ptr<Discovery>;
  // 回调函数的两个参数：服务信息名称 - 主机地址
  // 表明哪台主机上线了什么服务/哪台主机下线了什么服务
  using NotifyCallback = std::function<void(std::string, std::string)>;
  // 三个参数：etcd服务器地址，上线回调函数，下线回调函数
  Discovery(const std::string &host, NotifyCallback put_cb, NotifyCallback del_cb)
      : _client(std::make_shared<etcd::Client>(host)),
        _put_cb(put_cb), _del_cb(del_cb)
  {
    // 要求先获取到服务器上指定目录下的数据，再对该目录进行监控，所以不实例化watcher对象，一旦实例化就进行监控了
  }
  // 获取指定服务信息，指定目录即可
  bool discovery(const std::string &basedir)
  {
    auto resp = _client->ls(basedir).get(); // 可以获取对应目录下的所有key-value值
    /*/server/user-127.0.0.1:8080
      /server/friend-127.0.0.1:8085 */
    if (resp.is_ok() == false)
    {
      DEBUG_LOG("获取数据失败：{}", resp.error_message())
      return false;
    }
    // 获取成功，但是可能会获取该目录下的多个
    int sz = resp.keys().size();
    for (int i = 0; i < sz; i++)
    {
      // 第一次获取服务信息，如果有，就相当于已添加了服务信息，要对添加的服务信息做什么呢?由用户的回调函数决定
      if (_put_cb)
        _put_cb(resp.key(i), resp.value(i).as_string());
        //因为查找以主目录/service或者以/service/echo目录查找，所以下面的子服务实例都会被找到，然后传过去，服务形式传过去：/server/echo/instance1-127.0.0.1：7777
    }
    // 实例化一个监控对象用来监控其他客户端是否向服务器中指定服务目录添加或修改了或者删除了数据,如果发生变化则告诉该客户端
    _watcher = std::make_shared<etcd::Watcher>(*_client.get(), basedir, 
    std::bind(&Discovery::callback,this,std::placeholders::_1), true);
    //_client。get得到原生指针，解引用获取原对象
    //要求传入的回调函数是只有一个参数，而类内的成员函数有一个默认的this指针参数，先绑定进去。
    
    return true;
  }
  ~Discovery()
  {
    _watcher->Cancel();
  }
private:
  void callback(const etcd::Response &resp)
  {
    if (resp.is_ok() == false)
    {
      DEBUG_LOG( "收到一个错误的事件通知:{}" , resp.error_message())
      return;
    }
    // 收到正确的事件通知，可能会收到多个通知,因为一个目录下可能有多个服务信息
    for (auto const &ev : resp.events())
    {
      if (ev.event_type() == etcd::Event::EventType::PUT)
      {
        //新添加一个服务，针对是当前的新服务,进行处理
        if(_put_cb)_put_cb(ev.kv().key(),ev.kv().as_string());
        DEBUG_LOG("新增了一个服务：{}-{}",ev.kv().key(),ev.kv().as_string());
      }
      if (ev.event_type() == etcd::Event::EventType::DELETE_)
      {
        //删除一个服务，针对的是原先的服务，进行处理
        if(_del_cb)_del_cb( ev.prev_kv().key(),ev.prev_kv().as_string());
        DEBUG_LOG("删除了一个服务：{}-{}",ev.prev_kv().key(),ev.prev_kv().as_string());
      }
    }
  }

private:
  NotifyCallback _put_cb; // 服务上线事件回调接口
  NotifyCallback _del_cb; // 服务下线事件回调接口
  std::shared_ptr<etcd::Client> _client;
  std::shared_ptr<etcd::Watcher> _watcher;
};