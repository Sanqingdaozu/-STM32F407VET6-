package com.example.myapplication;

import androidx.appcompat.app.AppCompatActivity;

import android.Manifest;
import android.app.AlertDialog;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothServerSocket;
import android.bluetooth.BluetoothSocket;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.os.Bundle;
import androidx.appcompat.app.AppCompatActivity;

import android.view.LayoutInflater;
import android.view.View;
import android.widget.AdapterView;
import android.widget.Button;
import android.widget.ListView;
import android.widget.TextView;
import android.widget.Toast;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.List;
import java.util.Set;
import java.util.UUID;

public class MainActivity extends AppCompatActivity implements View.OnClickListener {

    private BluetoothAdapter bTAdatper;
    private ListView listView;
    private BlueToothDeviceAdapter adapter;
    private TextView selectedModeText;
    private TextView text_state;
    //private TextView text_msg;

    //private final int BUFFER_SIZE = 1024;
    private static final String NAME = "BT_DEMO";
    private static final UUID BT_UUID = UUID.fromString("00001101-0000-1000-8000-00805F9B34FB");

    private ConnectThread connectThread;
    private ListenerThread listenerThread;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);
        selectedModeText = findViewById(R.id.text_selected_mode);
        // 获取 BluetoothAdapter
        bTAdatper = BluetoothAdapter.getDefaultAdapter();
        if (bTAdatper == null) {
            Toast.makeText(this, "该设备不支持蓝牙", Toast.LENGTH_SHORT).show();
            finish(); // 直接退出
            return;
        }

        // 申请权限
        if (!checkAndRequestPermissions()) {
            return; // 等待用户授权，授权结果在 onRequestPermissionsResult 中处理
        }

        // 权限已全部获取，继续初始化
        initializeBluetoothApp();
    }
    private boolean checkAndRequestPermissions() {
        boolean needRequest = false;
        List<String> requestList = new ArrayList<>();

        // Android 6.0+ 定位权限
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M &&
                checkSelfPermission(Manifest.permission.ACCESS_FINE_LOCATION) != PackageManager.PERMISSION_GRANTED) {
            requestList.add(Manifest.permission.ACCESS_FINE_LOCATION);
            needRequest = true;
        }

        // Android 12+ 蓝牙权限
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            if (checkSelfPermission(Manifest.permission.BLUETOOTH_CONNECT) != PackageManager.PERMISSION_GRANTED) {
                requestList.add(Manifest.permission.BLUETOOTH_CONNECT);
                needRequest = true;
            }
            if (checkSelfPermission(Manifest.permission.BLUETOOTH_SCAN) != PackageManager.PERMISSION_GRANTED) {
                requestList.add(Manifest.permission.BLUETOOTH_SCAN);
                needRequest = true;
            }
        }

        if (needRequest) {
            requestPermissions(requestList.toArray(new String[0]), 100);
            return false;
        }

        return true;
    }
    @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults);

        boolean allGranted = true;
        for (int result : grantResults) {
            if (result != PackageManager.PERMISSION_GRANTED) {
                allGranted = false;
                break;
            }
        }

        if (allGranted) {
            initializeBluetoothApp();
        } else {
            Toast.makeText(this, "权限未授权，功能将无法使用", Toast.LENGTH_LONG).show();
        }
    }
    private void initializeBluetoothApp() {
        initView();
        initReceiver();
        listenerThread = new ListenerThread();
        listenerThread.start();
    }

    private void initView() {
        findViewById(R.id.btn_openBT).setOnClickListener(this);
        findViewById(R.id.btn_search).setOnClickListener(this);
        findViewById(R.id.btn_send).setOnClickListener(v -> {
            if (connectThread == null) {
                Toast.makeText(MainActivity.this, "请先连接蓝牙设备", Toast.LENGTH_SHORT).show();
                return;
            }
            showSendOptionDialog(); // 弹出选择对话框
        });
        text_state = (TextView) findViewById(R.id.text_state);
        //text_msg = (TextView) findViewById(R.id.text_msg);

        listView = (ListView) findViewById(R.id.listView);
        adapter = new BlueToothDeviceAdapter(getApplicationContext(), R.layout.bluetooth_device_list_item);
        listView.setAdapter(adapter);
        listView.setOnItemClickListener(new AdapterView.OnItemClickListener() {
            @Override
            public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
                if (bTAdatper.isDiscovering()) {
                    bTAdatper.cancelDiscovery();
                }
                BluetoothDevice device = (BluetoothDevice) adapter.getItem(position);
                //连接设备
                if (device.getBondState() == BluetoothDevice.BOND_BONDED) {
                    // 已配对，直接连接
                    connectDevice(device);
                } else {
                    // 未配对，尝试创建配对
                    Toast.makeText(MainActivity.this, "设备未配对，正在发起配对...", Toast.LENGTH_SHORT).show();
                    pairDevice(device);
                }

            }
        });
    }
    private void pairDevice(BluetoothDevice device) {
        try {
            Method method = device.getClass().getMethod("createBond");
            method.invoke(device);  // 反射调用配对
        } catch (Exception e) {
            e.printStackTrace();
            Toast.makeText(this, "配对失败", Toast.LENGTH_SHORT).show();
        }
    }
    private void showSendOptionDialog() {
        View dialogView = getLayoutInflater().inflate(R.layout.dialogue_send_option, null);
        AlertDialog dialog = new AlertDialog.Builder(this)
                .setView(dialogView)
                .create();

        Button btnColor = dialogView.findViewById(R.id.btn_color);
        Button btnHuman = dialogView.findViewById(R.id.btn_human_face);



        btnColor.setOnClickListener(v -> {
            if (connectThread != null) {
                connectThread.sendMsg("1");  // 发送识别色块信号
                selectedModeText.setText("已选择：识别色块");  // ✅ 在按钮旁边显示文字
            } else {
                Toast.makeText(MainActivity.this, "请先进行蓝牙连接", Toast.LENGTH_SHORT).show();
            }
            dialog.dismiss();
        });

        btnHuman.setOnClickListener(v -> {
            showFaceSelectionDialog(); // 不再直接发送"2"
            dialog.dismiss();
        });

        dialog.show();
    }
    private void showFaceSelectionDialog() {
        View view = LayoutInflater.from(this).inflate(R.layout.dialogue_select_face_type, null);
        AlertDialog dialog = new AlertDialog.Builder(this).setView(view).create();

        Button btnRandom = view.findViewById(R.id.btn_random_face);
        Button btnSpecific = view.findViewById(R.id.btn_specific_face);

        btnRandom.setOnClickListener(v -> {
            if (connectThread == null) {
                Toast.makeText(MainActivity.this, "请先进行蓝牙连接", Toast.LENGTH_SHORT).show();
            } else {
                connectThread.sendMsg("2");
                selectedModeText.setText("已选择：识别随机人脸");
                dialog.dismiss();
            }
        });

        btnSpecific.setOnClickListener(v -> {
            dialog.dismiss();
            showSpecificFaceDialog(); // 弹出特定人脸子界面
        });

        dialog.show();
    }

    private void showSpecificFaceDialog() {
        View view = LayoutInflater.from(this).inflate(R.layout.dialogue_specific_face, null);
        AlertDialog dialog = new AlertDialog.Builder(this).setView(view).create();

        view.findViewById(R.id.btn_face1).setOnClickListener(v -> {
            if (connectThread != null) connectThread.sendMsg("3");
            selectedModeText.setText("已选择：人脸1");
            dialog.dismiss();
        });

        view.findViewById(R.id.btn_face2).setOnClickListener(v -> {
            if (connectThread != null) connectThread.sendMsg("4");
            selectedModeText.setText("已选择：人脸2");
            dialog.dismiss();
        });

        view.findViewById(R.id.btn_face3).setOnClickListener(v -> {
            if (connectThread != null) connectThread.sendMsg("5");
            selectedModeText.setText("已选择：人脸3");
            dialog.dismiss();
        });

        dialog.show();
    }
    private void initReceiver() {
        //注册广播
        IntentFilter filter = new IntentFilter();
        filter.addAction(BluetoothDevice.ACTION_FOUND);
        filter.addAction(BluetoothAdapter.ACTION_DISCOVERY_STARTED);
        filter.addAction(BluetoothAdapter.ACTION_DISCOVERY_FINISHED);
        registerReceiver(mReceiver, filter);
        filter.addAction(BluetoothDevice.ACTION_BOND_STATE_CHANGED);
    }

    @Override
    public void onClick(View v) {
        switch (v.getId()) {
            case R.id.btn_openBT:
                openBlueTooth();
                break;
            case R.id.btn_search:
                searchDevices();
                break;
            case R.id.btn_send:
                if (connectThread != null) {
                    connectThread.sendMsg("这是蓝牙发送过来的消息");
                }
                break;
        }
    }


    /**
     * 开启蓝牙
     */
    private void openBlueTooth() {
        if (bTAdatper == null) {
            Toast.makeText(this, "当前设备不支持蓝牙功能", Toast.LENGTH_SHORT).show();
        }
        if (!bTAdatper.isEnabled()) {
           /* Intent i = new Intent(BluetoothAdapter.ACTION_REQUEST_ENABLE);
            startActivity(i);*/
            bTAdatper.enable();
        }
        //开启被其它蓝牙设备发现的功能
        if (bTAdatper.getScanMode() != BluetoothAdapter.SCAN_MODE_CONNECTABLE_DISCOVERABLE) {
            Intent i = new Intent(BluetoothAdapter.ACTION_REQUEST_DISCOVERABLE);
            //设置为一直开启
            i.putExtra(BluetoothAdapter.EXTRA_DISCOVERABLE_DURATION, 0);
            startActivity(i);
        }
    }

    /**
     * 搜索蓝牙设备
     */
    private void searchDevices() {
        if (bTAdatper.isDiscovering()) {
            bTAdatper.cancelDiscovery();
        }
        getBoundedDevices();
        bTAdatper.startDiscovery();
    }

    /**
     * 获取已经配对过的设备
     */
    private void getBoundedDevices() {
        //获取已经配对过的设备
        Set<BluetoothDevice> pairedDevices = bTAdatper.getBondedDevices();
        //将其添加到设备列表中
        if (pairedDevices.size() > 0) {
            for (BluetoothDevice device : pairedDevices) {
                adapter.add(device);
            }
        }
    }

    /**
     * 连接蓝牙设备
     */
    private void connectDevice(BluetoothDevice device) {

        text_state.setText(getResources().getString(R.string.connecting));

        try {
            //创建Socket
            BluetoothSocket socket = device.createRfcommSocketToServiceRecord(BT_UUID);
            //启动连接线程
            connectThread = new ConnectThread(socket, true);
            connectThread.start();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        //取消搜索
        if (bTAdatper != null && bTAdatper.isDiscovering()) {
            bTAdatper.cancelDiscovery();
        }
        //注销BroadcastReceiver，防止资源泄露
        unregisterReceiver(mReceiver);
    }

    private final BroadcastReceiver mReceiver = new BroadcastReceiver() {
        @Override
        public void onReceive(Context context, Intent intent) {
            String action = intent.getAction();
            if (BluetoothDevice.ACTION_FOUND.equals(action)) {
                BluetoothDevice device = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
                //避免重复添加已经绑定过的设备
                if (device != null && device.getName() != null) {
                    adapter.add(device);
                    adapter.notifyDataSetChanged();
                }
            } else if (BluetoothAdapter.ACTION_DISCOVERY_STARTED.equals(action)) {
                Toast.makeText(MainActivity.this, "开始搜索", Toast.LENGTH_SHORT).show();
            } else if (BluetoothAdapter.ACTION_DISCOVERY_FINISHED.equals(action)) {
                Toast.makeText(MainActivity.this, "搜索完毕", Toast.LENGTH_SHORT).show();
            }

            else if (BluetoothDevice.ACTION_BOND_STATE_CHANGED.equals(action)) {
                BluetoothDevice device = intent.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE);
                if (device.getBondState() == BluetoothDevice.BOND_BONDED) {
                    Toast.makeText(context, "配对成功，开始连接...", Toast.LENGTH_SHORT).show();
                    connectDevice(device); // 自动连接
                } else if (device.getBondState() == BluetoothDevice.BOND_NONE) {
                    Toast.makeText(context, "配对失败或取消", Toast.LENGTH_SHORT).show();
                }
            }
        }

    };

    /**
     * 连接线程
     */
    private class ConnectThread extends Thread {

        private BluetoothSocket socket;
        private boolean activeConnect;
        InputStream inputStream;
        OutputStream outputStream;

        private ConnectThread(BluetoothSocket socket, boolean connect) {
            this.socket = socket;
            this.activeConnect = connect;
        }

        @Override
        public void run() {
            try {
                //如果是自动连接 则调用连接方法
                if (activeConnect) {
                    socket.connect();
                }
                text_state.post(new Runnable() {
                    @Override
                    public void run() {
                        text_state.setText(getResources().getString(R.string.connect_success));
                    }
                });
                inputStream = socket.getInputStream();
                outputStream = socket.getOutputStream();


            } catch (IOException e) {
                e.printStackTrace();
                text_state.post(new Runnable() {
                    @Override
                    public void run() {
                        text_state.setText(getResources().getString(R.string.connect_error));
                    }
                });
            }
        }

        /**
         * 发送数据
         *
         * @param msg
         */
        public void sendMsg(final String msg) {

            byte[] bytes = msg.getBytes();
            if (outputStream != null) {
                try {
                    //发送数据
                    outputStream.write(bytes);

                } catch (IOException e) {
                    e.printStackTrace();

                }
            }
        }
    }

    /**
     * 监听线程
     */
    private class ListenerThread extends Thread {

        private BluetoothServerSocket serverSocket;
        private BluetoothSocket socket;

        @Override
        public void run() {
            try {
                serverSocket = bTAdatper.listenUsingRfcommWithServiceRecord(
                        NAME, BT_UUID);
                while (true) {
                    //线程阻塞，等待别的设备连接
                    socket = serverSocket.accept();
                    text_state.post(new Runnable() {
                        @Override
                        public void run() {
                            text_state.setText(getResources().getString(R.string.connecting));
                        }
                    });
                    connectThread = new ConnectThread(socket, false);
                    connectThread.start();
                }
            } catch (IOException e) {
                e.printStackTrace();
            }
        }
    }
}