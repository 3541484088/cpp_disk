#pragma once
class XUploadProxy
{
public:
    static XUploadProxy*Get()
    {
        static XUploadProxy xp;
        return &xp;
    }
    void Start();
    
    ~XUploadProxy();
private:
    void Main();
    bool is_exit_ = false;
    XUploadProxy()
    {
        
    }
};

