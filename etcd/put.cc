#include<etcd/Client.hpp>
#include<etcd/KeepAlive.hpp>
#include<etcd/Response.hpp>
#include<unistd.h>
int main()
{
    //定义服务器地址
    const std::string etcd_host="http://127.0.0.1:2379";
    //首先实例化一个客户端对象，用来连接etcd服务器
    etcd::Client client(etcd_host);
    //获取该该客户端租约保护对象（智能指针），--获取的过程中就会创建一个指定时长的租约
    auto keep_alive=client.leasekeepalive(3).get();//每3秒交一次租
    //有了保活对象，就可以获取对应的租约id
    auto lease_id=keep_alive->Lease();
    //然后向服务器添加一个键值对数据
    auto resp1=client.put("/server/user","127.0.0.1:8080",lease_id).get();//该数据就会按照该租约形式续租
    if(resp1.is_ok()==false)
    {
        std::cout<<"添加数据失败:"<<resp1.error_message()<<std::endl;
        return -1;
    }
    auto resp2=client.put("/server/friend","127.0.0.1:8085").get();//不添加租约id，就不受租约限制
    if(resp2.is_ok()==false)
    {
        std::cout<<"添加数据失败:"<<resp2.error_message()<<std::endl;
        return -1;
    }
    //添加成功
    sleep(10);
}