#include<etcd/Client.hpp>
#include<etcd/Watcher.hpp>
#include<etcd/Value.hpp>
#include<etcd/Response.hpp>

void callback(const etcd::Response&resp)
{
    if(resp.is_ok()==false)
    {
        std::cout<<"收到一个错误的事件通知"<<resp.error_message()<<std::endl;
        return;
    }
    //收到正确的事件通知，可能会收到多个通知
    for(auto const&ev:resp.events())
    {
        if(ev.event_type()==etcd::Event::EventType::PUT)
        {
            std::cout<<"服务信息发生了改变：\n";
            /*/原本的：server/user-127.0.0.1:8080
              现在的：/server/user-127.0.0.1:5555*/
            std::cout<<"当前的服务："<<ev.kv().key()<<"-"<<ev.kv().as_string()<<std::endl;
            std::cout<<"原先的服务："<<ev.prev_kv().key()<<"-"<<ev.prev_kv().as_string()<<std::endl;
        }
        if(ev.event_type()==etcd::Event::EventType::DELETE_)
        {
             std::cout<<"服务信息发生了改变：\n";
            /*/原本的：server/user-127.0.0.1:8080
              现在的：/server/user-*/
            std::cout<<"当前的服务："<<ev.kv().key()<<"-"<<ev.kv().as_string()<<std::endl;
            std::cout<<"原先的服务："<<ev.prev_kv().key()<<"-"<<ev.prev_kv().as_string()<<std::endl;
        }
    }

}
int main()
{
    const std::string etcd_host="http://127.0.0.1:2379";
    //首先实例化一个客户端对象用来连接etcd服务器
    etcd::Client client(etcd_host);
    //获取指定的键值对数据
    auto resp=client.ls("/server").get();//可以获取对应server目录下的所有key-value值
    /*/server/user-127.0.0.1:8080 
      /server/friend-127.0.0.1:8085 */
    if(resp.is_ok()==false)
    {
        std::cout<<"获取数据失败："<<resp.error_message()<<std::endl;
    }
    //获取成功，但是可能会获取该目录下的多个
    int sz=resp.keys().size();
    for(int i=0;i<sz;i++)
    {
        std::cout<<resp.value(i).as_string()<<"可以提供"<<resp.key(i)<<"功能"<<std::endl;
    }
    //实例化一个监控对象用来监控其他客户端是否向服务器中添加或修改了或者删除了数据,如果发生变化则告诉该客户端
    auto watcher=etcd::Watcher(client,"/server",callback,true);
    watcher.Wait();
    return 0;
}