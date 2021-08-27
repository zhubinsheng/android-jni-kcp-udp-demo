package com.fotric.kcp;

import androidx.appcompat.app.AppCompatActivity;

import android.net.DhcpInfo;
import android.os.Bundle;
import android.os.Environment;
import android.util.Log;
import android.widget.TextView;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.ServerSocket;
import java.net.Socket;

public class MainActivity extends AppCompatActivity {

    // Used to load the 'native-lib' library on application startup.
    static {
        System.loadLibrary("kcppClient");
        System.loadLibrary("native-lib");
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        // Example of a call to a native method
        TextView tv = findViewById(R.id.sample_text);
        new Thread(new Runnable() {
            @Override
            public void run() {
                //        receiveFile();

//                    sendFile("/storage/emulated/0/out.h264","test");
            }
        }).start();
    }

    /**
     * A native method that is implemented by the 'native-lib' native library,
     * which is packaged with this application.
     */
    public native String stringFromJNI();

    int PORT = 6789;

    /*
     * @param path     文件路径
     * @param FileName 文件名
     */
    public void sendFile(String path, String FileName) throws IOException {
        Socket s = new Socket("192.168.12.102", PORT);
        OutputStream out = s.getOutputStream();
        //将文件名写在流的头部以#分割
        out.write((FileName + "#").getBytes());
        FileInputStream inputStream = new FileInputStream(new File(path));
        byte[] buf = new byte[1024];
        int len;
        //判断是否读到文件末尾
        Log.e("time",String.valueOf(System.currentTimeMillis()));
        long ttt = System.currentTimeMillis();
        while ((len = inputStream.read(buf)) != -1) {
            Log.e("consume time",String.valueOf(System.currentTimeMillis() - ttt));
            out.write(buf, 0, len);//将文件循环写入输出流
        }
        //告诉服务端，文件已传输完毕
        s.shutdownOutput();
        Log.e("consume time",String.valueOf(System.currentTimeMillis() - ttt));

        //获取从服务端反馈的信息
        BufferedReader in = new BufferedReader(new InputStreamReader(s.getInputStream()));
        String serverBack = in.readLine();
        Log.d("TAG", serverBack);
        //资源关闭
        s.close();
        inputStream.close();

    }

    public synchronized void receiveFile() {
        try {
            ServerSocket ss = new ServerSocket(PORT);
            while (true) {
                Socket socket = ss.accept();
                InputStream in = socket.getInputStream();
                int content;
                //装载文件名的数组
                byte[] c = new byte[1024];
                //解析流中的文件名,也就是开头的流
                for (int i = 0; (content = in.read()) != -1; i++) {
                    //表示文件名已经读取完毕
                    if (content == '#') {
                        break;
                    }
                    c[i] = (byte) content;
                }
                //将byte[]转化为字符,也就是我们需要的文件名
                String FileName = new String(c, "utf-8").trim();
                //创建一个文件,指定保存路径和刚才传输过来的文件名
                OutputStream saveFile = new FileOutputStream(
                        new File(Environment.getExternalStorageDirectory().toString(), FileName));
                byte[] buf = new byte[1024];
                int len;
                //判断是否读到文件末尾
                while ((len = in.read(buf)) != -1) {
                    saveFile.write(buf, 0, len);
                }
                saveFile.flush();
                saveFile.close();
                //告诉发送端我已经接收完毕
                OutputStream outputStream = socket.getOutputStream();
                outputStream.write("文件接收成功".getBytes());
                outputStream.flush();
                outputStream.close();
                socket.close();
            }
        } catch (Exception e) {
            e.printStackTrace();
        }
    }
}