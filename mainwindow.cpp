#include "mainwindow.h"
#include "ui_mainwindow.h"
#define CAMERA_FRESH_TIME 45
#define CALIBRATE_UPDATE_TIME 2000
#define COLOR_DISTANCE_THRESHOLD 0.4
#define TAN_20 0.36396471
int image_num = 0;
//eeeee
QFile file("./color.txt");
QTextStream stream(&file);

QFile file_gray("./output.txt");

QTextStream out(&file_gray);

int gray_count=0;

struct view_zone_data{
    Mat full_image;
    Mat lb;
    Mat lw;
    Mat rb;
    Mat rw;
    double cross_talk_right;
    double cross_talk_left;
};


std::vector<view_zone_data> positive;
std::vector<view_zone_data> negative;
int count_view = 16;
double positive_view_length = 0.0;
double negative_view_length = 0.0;
bool find_positive_edge = false;
bool find_negative_edge = false;
double tmp_crosstalk_right;
double tmp_crosstalk_left;

std::vector<double> x_filter;
std::vector<double> vd_filter;



MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
   // ui->stackedWidget->setAnimation(QEasingCurve::Type::OutQuart);
   this->setFixedSize(this->geometry().width(), this->geometry().height());

    camera_fresh_timer = new QTimer(this);
    camera_fresh_timer->setInterval(CAMERA_FRESH_TIME);
    connect(camera_fresh_timer, SIGNAL(timeout()), this, SLOT(preview_camera()));

    calibrate_timer = new QTimer(this);
    calibrate_timer->setInterval(CALIBRATE_UPDATE_TIME);
    calibrate_timer->setSingleShot(true);

    Eye_Tracking_timer = new QTimer(this);
    Eye_Tracking_timer->setInterval(100);
    connect(Eye_Tracking_timer, SIGNAL(timeout()), this, SLOT(eye_tracking_ui()));

    hcp_slant_step = new QSignalMapper(this);
    connect(hcp_slant_step, SIGNAL(mapped(int)), this, SLOT(calibrate_hcp(int)));
    xoff_lens_step = new QSignalMapper(this);
    connect(xoff_lens_step, SIGNAL(mapped(int)), this, SLOT(calibrate_xoff_lens(int)));
    ws_ovd_step = new QSignalMapper(this);
    connect(ws_ovd_step, SIGNAL(mapped(int)), this, SLOT(calibrate_ws_ovd(int)));
    view_zone_step = new QSignalMapper(this);
    connect( view_zone_step, SIGNAL(mapped(int)), this, SLOT(calibrate_view_zone(int)));
    vd_it_step = new QSignalMapper(this);
    connect( vd_it_step, SIGNAL(mapped(int)), this, SLOT(calibrate_vd(int)));

    connect(this,SIGNAL(camera_loss()),this,SLOT(calibrate_terminate()));

    initial();



}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent* event){

    QMessageBox::StandardButton reply;
           reply = QMessageBox::question(this, "Exit", "Are tou Leaveing?",
                                         QMessageBox::Yes | QMessageBox::No);
           if (reply == QMessageBox::Yes) {
               // 用户选择关闭，关闭窗口
               single.is_close = true;
               dual_1.is_close = true;
               dual_2.is_close = true;
               camera_fresh_timer->stop();
               if(calibrate_mode==0){
                   second_monitor.widget->close();
               }else if(calibrate_mode==1){
                   AUO3D_stop();
               }
               event->accept();

           } else {
               // 用户选择取消关闭，忽略关闭事件
               event->ignore();
           }

}

//---------------------------------------------------Initial all page
void MainWindow::initial(){
    //system check
     ui->stackedWidget->setCurrentIndex(0);
     ui->Calibration_Function_Box->hide();
     ui->fpga_status->hide();
     ui->pc_status->hide();
     ui->calibrate_next->hide();
     QList<QScreen*> screens = QGuiApplication::screens();
     second_monitor.widget = new QWidget();
     second_monitor.label = new QLabel(second_monitor.widget);
     Second_Monitor_mode(R_G);
     second_monitor.widget->show();


    QDesktopWidget dw;
    if (dw.screenCount() > 1) {
            //ui.Log->setPlainText("Monitor Check .....Detect Second Monitor.");

            second_monitor.widget->windowHandle()->setScreen(qApp->screens()[0]);

        }
        else {
            second_monitor.widget->windowHandle()->setScreen(qApp->screens()[0]);
        }
    second_monitor.widget->showFullScreen();
    second_monitor.widget->hide();

    connect(ui->calibrate_next, &QPushButton::clicked, this, &MainWindow::to_next_calibrate_step);

    //Page 0
         //check screen
         if (screens.size() > 1) {
             QScreen *screen = screens[1];
             resolution = screen->size();
         } else {
             QScreen *screen = screens[0];
             resolution = screen->size();
             ui->warning_monitor->setText(QString::fromLocal8Bit("Warning:  偵測到只使用一個螢幕，校正可能無法正常進行"));
         }
    //Page 0
        //select mode
        QPalette button_palette;
        QPixmap FPGA_img = load_image("./FPGA.png",ui->FPGA_btn->width(),ui->FPGA_btn->height());
        QPixmap PC_img = load_image("./PC_clear.png",ui->PC_btn->width(),ui->PC_btn->height());
        ui->FPGA_btn->setIcon(QIcon(FPGA_img));
        ui->FPGA_btn->setIconSize(QSize(FPGA_img.width()-10,FPGA_img.height()-10));
        ui->PC_btn->setIcon(QIcon(PC_img));
        ui->PC_btn->setIconSize(QSize(PC_img.width()-10,PC_img.height()-10));      
        ui->back->hide();      
        connect(ui->FPGA_btn, &QPushButton::clicked, this, &MainWindow::select_FPGA);
        connect(ui->PC_btn, &QPushButton::clicked, this, &MainWindow::select_PC);

    //Page 1
        //connect
        connect(ui->back, &QPushButton::clicked, this, &MainWindow::page_dec);
        connect(ui->fpga_connect, &QPushButton::clicked, this, &MainWindow::connect_fpga_);
        connect(ui->start_calibration, &QPushButton::clicked, this, &MainWindow::start_calibrate);
        connect(ui->load_parameters,&QPushButton::clicked, this, &MainWindow::load_ini_file);

        //Page 9
        QPixmap panel_pos = load_image("./Panel_Align.jpg",ui->Panel_pos->width(),ui->Panel_pos->height());
        QPixmap pos_align = load_image("./Pos_align.jpg",ui->Panel_pos->width(),ui->Panel_pos->height());
        QPixmap center_pos = load_image("./Center_Align.jpg",ui->Panel_pos->width(),ui->Panel_pos->height());
        QPixmap center_pos2 = load_image("./Center_Align2.jpg",ui->Panel_pos->width(),ui->Panel_pos->height());
        QPixmap board_align = load_image("./Board_Align.jpg",ui->Panel_pos->width(),ui->Panel_pos->height());
        ui->Board_pos->setPixmap(board_align);
         ui->Panel_pos->setPixmap(panel_pos);
          ui->Label_pos->setPixmap(pos_align);
           ui->Center_pos->setPixmap( center_pos);
           ui->Center_pos_2->setPixmap(center_pos2);
           connect(ui->camera_check_box, &QCheckBox::stateChanged, this, &MainWindow::final_check);
           connect(ui->camera_check, &QPushButton::clicked, this, &MainWindow::check_content_switch_RG);
           connect(ui->human_check_box, &QCheckBox::stateChanged, this, &MainWindow::final_check);
           connect(ui->human_check, &QPushButton::clicked, this, &MainWindow::check_content_switch_Detail);


        //Page 2
        //camera
         QPixmap dual_img = load_image("./dual.png",ui->single_img->width(),ui->single_img->height());
         QPixmap single_img = load_image("./single.png",ui->dual_img->width(),ui->dual_img->height());
         QPixmap file_img = load_image("./file.png",ui->load_file->width(),ui->load_file->height());
         ui->single_img->setPixmap(single_img);
         ui->dual_img->setPixmap(dual_img);
         ui->load_file->setPixmap(file_img);
         connect(ui->reconnect_camera, &QPushButton::clicked, this, &MainWindow::reconnect_camera);
         //connect(ui->cam_next, &QPushButton::clicked, this, &MainWindow::to_hcp_slant_page);
         connect(ui->cam_next, &QPushButton::clicked, this, &MainWindow::to_env_setting_page);


    //page 3
        //HCP slant
         QPixmap camera_type = load_image("./single.png",ui->slant_cam_type->width(),ui->slant_cam_type->height());
         QPixmap camera_position = load_image("./camera_position.png",ui->camera_position->width(),ui->camera_position->height());
         QPixmap rg_screen= load_image("./camera_screen.png",ui->initial_screen_hcp->width(),ui->initial_screen_hcp->height());
         ui->slant_cam_type->setPixmap(camera_type);
         ui->camera_position->setPixmap(camera_position);
         //tmp delete
         ui->label_41->hide();
         ui->camera_position->hide();

         ui->initial_screen_hcp->setPixmap(rg_screen);
         connect(ui->camera_setting, &QPushButton::clicked, this, &MainWindow::to_camera_setting_page);
         connect(ui->start_calibrate_hcp, &QPushButton::clicked, this, &MainWindow::start_hcp);
         connect(ui->hcp_slant, &QPushButton::clicked, this, &MainWindow::to_hcp_slant_page);

    //page 4
         //xoff lens
         camera_type = load_image("./single.png",ui->xoff_cam_type->width(),ui->xoff_cam_type->height());
         camera_position = load_image("./camera_position.png",ui->xoff_camera_position->width(),ui->xoff_camera_position->height());
         rg_screen= load_image("./xoff_alighment.png",ui->initial_screen_xoff_lens->width(),ui->initial_screen_xoff_lens->height());
         ui->xoff_cam_type->setPixmap(camera_type);
         ui->xoff_camera_position->setPixmap(camera_position);

         //tmp delete
         ui->label_51->hide();
         ui->xoff_camera_position->hide();

         ui->initial_screen_xoff_lens->setPixmap(rg_screen);
         connect(ui->start_calibrate_xoff_lens, &QPushButton::clicked, this, &MainWindow::start_xoff_lens);
         connect(ui->Xoff_len,&QPushButton::clicked, this, &MainWindow::to_xoff_len_page);
    //page 5
         //ws ovd
        camera_type = load_image("./single.png",ui->slant_cam_type->width(),ui->slant_cam_type->height());
        camera_position = load_image("./camera_position.png",ui->camera_position->width(),ui->camera_position->height());
        rg_screen= load_image("./ws_ini.png",ui->initial_screen_hcp->width(),ui->initial_screen_hcp->height());
        ui->ws_cam_type->setPixmap(camera_type);
        ui->ws_camera_position->setPixmap(camera_position);

        //tmp delete
        ui->label_59->hide();
        ui->ws_camera_position->hide();

        ui->initial_screen_ws_ovd->setPixmap(rg_screen);     
        connect(ui->start_calibrate_ws_ovd, &QPushButton::clicked, this, &MainWindow::start_ws_ovd);
        connect(ui->to_ws_ovd,&QPushButton::clicked, this, &MainWindow::to_ws_ovd_page);
        //connect(ui->WS_TEST, &QPushButton::clicked, this, &MainWindow::ws_test);

    //page 6
        //view zone
        camera_type = load_image("./dual.png",ui->view_type->width(),ui->view_type->height());
        camera_position = load_image("./dual_pos.png",ui->view_pos->width(),ui->view_pos->height());
        rg_screen= load_image("./dual_ref.png",ui->view_ref->width(),ui->view_ref->height());
        ui->view_type->setPixmap(camera_type);
        ui->view_pos->setPixmap(camera_position);

        //tmp delete
        ui->label_66->hide();
        ui->view_pos->hide();

        ui->view_ref->setPixmap(rg_screen);
        connect(ui->start_calibrate_view_zone, &QPushButton::clicked, this, &MainWindow::start_view_zone);
        connect(ui->to_view_zone,&QPushButton::clicked, this, &MainWindow::to_view_zone_test_page);
    //page 7
        //vd it
        camera_type = load_image("./dual.png",ui->vd_type->width(),ui->vd_type->height());
        //camera_position = load_image("./1.png",ui->vd_pos->width(),ui->vd_pos->height());
        ui->vd_type->setPixmap(camera_type);
        //ui->vd_pos->setPixmap(camera_position);
        connect(ui->start_calibrate_vd, &QPushButton::clicked, this, &MainWindow::start_vd);
        connect(ui->to_vd_it,&QPushButton::clicked, this, &MainWindow::to_vd_it_page);
    //page 8
        //final
        camera_type=load_image("./eMMC.png",ui->load_to_emmc->width(),ui->load_to_emmc->height());
        QPixmap out_put = load_image("./file.png",ui->ouput_par->width(),ui->ouput_par->height());
        ui->ouput_par->setPixmap(out_put);
        ui->load_to_emmc->setPixmap(camera_type);
        connect(ui->save_result, &QPushButton::clicked, this, &MainWindow::save_file);
        connect(ui->to_emmc, &QPushButton::clicked, this, &MainWindow::load_to_emmc);       
        connect(ui->end_calibration, &QPushButton::clicked, this, &MainWindow::close);
        ui->end_calibration->hide();
        connect(ui->back_to_calibration,&QPushButton::clicked,this, &MainWindow::back_to_calibration);

        QPixmap right_correct_1 = load_image("./right_correct_1.png",ui->right_correct_1->width(),ui->right_correct_1->height());
        QPixmap right_correct_2 = load_image("./right_correct_2.png",ui->right_correct_2->width(),ui->right_correct_2->height());
        QPixmap left_correct_1 = load_image("./left_correct_1.png",ui->left_correct_1->width(),ui->left_correct_1->height());
        QPixmap left_correct_2 = load_image("./left_correct_2.png",ui->left_correct_2->width(),ui->left_correct_2->height());
        QPixmap right_wrong_1 = load_image("./right_wrong_1.png",ui->right_wrong_1->width(),ui->right_wrong_1->height());
        QPixmap left_wrong_1 = load_image("./left_wrong_1.png",ui->left_wrong_1->width(),ui->left_wrong_1->height());
        ui->right_correct_1->setPixmap(right_correct_1);
        ui->right_correct_2->setPixmap(right_correct_2);
        ui->left_correct_1->setPixmap(left_correct_1);
        ui->left_correct_2->setPixmap(left_correct_2);
        ui->right_wrong_1->setPixmap(right_wrong_1);
        ui->left_wrong_1->setPixmap(left_wrong_1);


        //ui->to_vd_it->hide();
        ui->to_ws_ovd->hide();
        ui->verticalGroupBox_12->hide();
        ui->verticalGroupBox_11->hide();
        ui->verticalGroupBox_14->hide();

        connect(ui->x_plus, &QPushButton::clicked, this, &MainWindow::set_position);
         connect(ui->x_dec, &QPushButton::clicked, this, &MainWindow::set_vd_par);
}

void MainWindow::set_position(){


    my_vd_it.set_x = ui->x_mm->text().toDouble();
    my_vd_it.set_y = ui->y_mm->text().toDouble();
    my_vd_it.set_z = ui->vd_mm->text().toDouble();

    if(calibrate_mode==0){
        //Second_Monitor_mode(R_R);
        AUO3D_FPGA_SendData(PanelData::X, &my_vd_it.set_x);
        AUO3D_FPGA_SendData(PanelData::Y, &my_vd_it.set_y);
        AUO3D_FPGA_SendData(PanelData::VD,&my_vd_it.set_z);
    }else if(calibrate_mode==1){
        AUO3D_SendData(PanelData::X, &my_vd_it.set_x);
        AUO3D_SendData(PanelData::Y, &my_vd_it.set_y);
        AUO3D_SendData(PanelData::VD,&my_vd_it.set_z);
    }

 }

void MainWindow::set_vd_par(){

    if(calibrate_mode==0){
        //Second_Monitor_mode(R_R);
        parameters.A = ui->A_value->text().toDouble();
        parameters.B = ui->B_value->text().toDouble();
        parameters.C = ui->C_value->text().toDouble();
        parameters.E1 = ui->E1_value->text().toDouble();
        parameters.E2 = ui->E2_value->text().toDouble();


        AUO3D_FPGA_SendData(PanelData::A, &parameters.A);
        AUO3D_FPGA_SendData(PanelData::B, &parameters.B);
        AUO3D_FPGA_SendData(PanelData::C, &parameters.C);
        AUO3D_FPGA_SendData(PanelData::E1,&parameters.E1);
        AUO3D_FPGA_SendData(PanelData::E2,&parameters.E2);

    }else if(calibrate_mode==1){
        parameters.A = ui->A_value->text().toDouble();
        parameters.B = ui->B_value->text().toDouble();
        parameters.C = ui->C_value->text().toDouble();
        parameters.E1 = ui->E1_value->text().toDouble();
        parameters.E2 = ui->E2_value->text().toDouble();

       AUO3D_SendData(PanelData::A, &parameters.A);
        AUO3D_SendData(PanelData::B, &parameters.B);
        AUO3D_SendData(PanelData::C, &parameters.C);
        AUO3D_SendData(PanelData::E1,&parameters.E1);
        AUO3D_SendData(PanelData::E2,&parameters.E2);
    }

 }


//--------------------------------------------------- Lib Function

void MainWindow::pc_is_running(){
    while(pc_status){
        if(AUO3D_isRunning()){
            pc_status = true;

        }else{
            pc_status = false;
            ui->pc_status->setStyleSheet("QLabel { color : red; }");
            ui->pc_status->setText(QString::fromLocal8Bit("PC 校正模組:: 斷線"));
            break;
        }
        Sleep(1500);
    }
}

void MainWindow::fpga_is_running(){
    while(fpga_status){
        if(fpga_eye_tracking.x!=-1){
            fpga_status = true;
            ui->fpga_status->setStyleSheet("QLabel { color : green; }");
            ui->fpga_status->setText(QString::fromLocal8Bit("FPGA 校正模組:: 已連線"));

        }else{
            fpga_status = false;
            ui->fpga_status->setStyleSheet("QLabel { color : red; }");
            ui->fpga_status->setText(QString::fromLocal8Bit("FPGA 校正模組:: 斷線"));
           // break;
        }
        Sleep(1500);
    }
}

void MainWindow::check_camera_device(){
    //enum camera name
       DeviceEnumerator de;
       // Video Devices
       if(parameters.model_type==-1){
           return;
       }
       std::map<int, Device> devices = de.getVideoDevicesMap(parameters.model_type);
       // Print information about the devices
       for (auto const &device : devices) {
           cout<<device.second.deviceName<<endl;

            if(device.second.deviceName=="Etron"){
               ui->single_camera_name->setStyleSheet("QLabel { color : white; }");
               ui->single_camera_name->setText(QString::fromLocal8Bit("已偵測: ")+QString::fromStdString(device.second.deviceName));
               single.id = device.first;
            }

           if(device.second.deviceName=="Adesso CyberTrack 6S "){
               //ui->single_cam_list->addItem(QString::fromStdString(device.second.deviceName)+"camera id:"+QString::number(device.first));
                ui->single_camera_name->setStyleSheet("QLabel { color : white; }");
               ui->single_camera_name->setText(QString::fromLocal8Bit("已偵測: ")+QString::fromStdString(device.second.deviceName));
               single.id = device.first;

           }
           else if(device.second.deviceName=="MicrosoftR LifeCam Studio(TM)"){
               if(dual_1.id ==-1){
                    ui->dual_camera_name_1->setStyleSheet("QLabel { color : white; }");
                   ui->dual_camera_name_1->setText(QString::fromLocal8Bit("已偵測: ")+QString::fromStdString(device.second.deviceName));
                   dual_1.id = device.first;

               }else if(dual_2.id ==-1){
                   ui->dual_camera_name_2->setStyleSheet("QLabel { color : white; }");
                   ui->dual_camera_name_2->setText(QString::fromLocal8Bit("已偵測: ")+QString::fromStdString(device.second.deviceName));
                   dual_2.id = device.first;
               }
           }

       }

       if(single.id==-1){
           ui->single_camera_name->setStyleSheet("QLabel { color : red; }");
           ui->single_camera_name->setText(QString::fromLocal8Bit("未偵測到裝置"));
       }else if( dual_1.id==-1){
            ui->dual_camera_name_1->setStyleSheet("QLabel { color : red; }");
            ui->dual_camera_name_1->setText(QString::fromLocal8Bit("未偵測到裝置"));
       }else if(dual_2.id ==-1){
           ui->dual_camera_name_2->setStyleSheet("QLabel { color : red; }");
           ui->dual_camera_name_2->setText(QString::fromLocal8Bit("未偵測到裝置"));
       }else{

           //open stream
           open_single_camera();
           open_dual_camera_1();
           open_dual_camera_2();

           //check camera stream
           check_camera_stream_thread();
           camera_fresh_timer->start();
       }

       std::cout<<"single id: "<<single.id<<std::endl<<"dual_camera_1 id: "<<dual_1.id
               <<std::endl<<"dual camera 2 id: "<<dual_2.id<<std::endl;


}

void MainWindow::check_camera_stream(){

    clock_t call_start, call_time;
    call_start = clock();
    while(1){

        if(!single.is_close){
            ui->single_status->setStyleSheet("QLabel { color : white; }");
            ui->single_status->setText(QString::fromLocal8Bit("白屏拍攝相機: 已連線"));
        }
        if(!dual_1.is_close){
            ui->dual_1_status->setStyleSheet("QLabel { color : white; }");
            ui->dual_1_status->setText(QString::fromLocal8Bit("雙眼模擬相機_1: 已連線"));
        }
        if(!dual_2.is_close){
            ui->dual_2_status->setStyleSheet("QLabel { color : white; }");
            ui->dual_2_status->setText(QString::fromLocal8Bit("雙眼模擬相機_2: 已連線"));
        }
        //check alive
        if(!single.is_close && !dual_1.is_close && !dual_2.is_close){
            ui->cam_next->setEnabled(true);
            ui->cam_next->setStyleSheet("background-color: rgb(103, 103, 103);");
            break;
        }
        call_time = clock();
        if((double(call_time) - double(call_start)) >3000) {
            if(single.is_close){
                ui->single_status->setStyleSheet("QLabel { color : red; }");
                ui->single_status->setText(QString::fromLocal8Bit("白屏拍攝相機: 斷線"));
            }
            if(dual_1.is_close){
                ui->dual_1_status->setStyleSheet("QLabel { color : red; }");
                ui->dual_1_status->setText(QString::fromLocal8Bit("雙眼模擬相機_1: 斷線"));
            }
            if(dual_2.is_close){
                ui->dual_2_status->setStyleSheet("QLabel { color : red; }");
                ui->dual_2_status->setText(QString::fromLocal8Bit("雙眼模擬相機_2: 斷線"));
            }
            break;
        }
        Sleep(300);

    }

    while(1){

        if(single.is_close){
           ui->single_status->setStyleSheet("QLabel { color : red; }");
           ui->single_status->setText(QString::fromLocal8Bit("白屏拍攝相機: 斷線"));
           ui->cam_next->setEnabled(true); //false
           emit camera_loss();
           break;
        }
        else if(dual_1.is_close){
            ui->dual_1_status->setStyleSheet("QLabel { color : red; }");
            ui->dual_1_status->setText(QString::fromLocal8Bit("雙眼模擬相機_1: 斷線"));
            ui->cam_next->setEnabled(false); //false
            emit camera_loss();
            break;
        }
        else if(dual_2.is_close){
            ui->dual_2_status->setStyleSheet("QLabel { color : red; }");
            ui->dual_2_status->setText(QString::fromLocal8Bit("雙眼模擬相機_2: 斷線"));
            ui->cam_next->setEnabled(false); //false
            emit camera_loss();
            break;
        }
        Sleep(300);
    }

}

void MainWindow::calibrate_terminate(){
    calibrate_timer->stop();
    ui->Warning->setText(QString::fromLocal8Bit("校正中斷:相機斷線"));
    ui->Calibration_Function_Box->setEnabled(true);
    if(my_hcp_slant.is_calibrating){
        my_hcp_slant.is_calibrating = false;
        ui->start_calibrate_hcp->setText(QString::fromLocal8Bit("開始校正"));
        ui->start_calibrate_hcp->setEnabled(true);
        disconnect(calibrate_timer, SIGNAL(timeout()),  hcp_slant_step, SLOT(map()));
    }else if(my_xoff_lens.is_calibrating){
        my_xoff_lens.is_calibrating = false;
        ui->start_calibrate_xoff_lens->setText(QString::fromLocal8Bit("開始校正"));
        ui->start_calibrate_xoff_lens->setEnabled(true);
        disconnect(calibrate_timer, SIGNAL(timeout()),  xoff_lens_step, SLOT(map()));
    }

}

int MainWindow::load_parameters(){


    if(ui->hcp_adjust_ini->text().isEmpty()&&ui->slant_adjust_ini->text().isEmpty()){
        ui->page_2_warning->setText(QString::fromLocal8Bit("所有校正參數需給值"));
        return 0;
    }else{
        my_hcp_slant.hcp_adjust_value = ui->hcp_adjust_ini->text().toDouble();
         my_hcp_slant.slant_adjust_value = ui->slant_adjust_ini->text().toDouble();
    }


        if(ui->h_max->text().isEmpty()&&ui->h_min->text().isEmpty()
                &&ui->v_min->text().isEmpty()&&ui->v_max->text().isEmpty()
                &&ui->vd_min->text().isEmpty()&&ui->vd_max->text().isEmpty()
                &&ui->cam_x->text().isEmpty()&&ui->cam_y->text().isEmpty()
                &&ui->cam_z->text().isEmpty()&&ui->rsl_x->text().isEmpty()
                &&ui->rsl_y->text().isEmpty()&&ui->ovd->text().isEmpty()
                &&ui->vd_far->text().isEmpty())
        {
            ui->page_2_warning->setText(QString::fromLocal8Bit("所有校正參數需給值"));
            return 0;
        }
        else{

            if(ui->slant_ini->text().isEmpty()&&ui->hcp_ini->text().isEmpty()
                    &&ui->xoff_lens_ini->text().isEmpty()&&ui->ws_ovd_ini->text().isEmpty()
                    &&ui->a_ini->text().isEmpty()&&ui->b_ini->text().isEmpty()
                    &&ui->c_ini->text().isEmpty()&&ui->e1_ini->text().isEmpty()
                    &&ui->e2_ini->text().isEmpty()  &&ui->hcp_adjust_ini->text().isEmpty()
                    &&ui->slant_adjust_ini->text().isEmpty()
                    &&ui->xoff_adjust_step->text().isEmpty()
                    &&ui->ws_ovd_adjust_step->text().isEmpty()
                    &&ui->view_zone_step->text().isEmpty()
                    &&ui->vd_step->text().isEmpty()
                    &&ui->it_step->text().isEmpty()
                &&ui->panel_id->text().isEmpty()
                &&ui->module_type->text().isEmpty())
            {
                ui->page_2_warning->setText(QString::fromLocal8Bit("所有校正參數需給值"));
                return 0;

            }else{
                parameters.fov_h_max = ui->h_max->text().toDouble();
                parameters.fov_h_min = ui->h_min->text().toDouble();
                parameters.fov_v_min = ui->v_min->text().toDouble();
                parameters.fov_v_min = ui->v_max->text().toDouble();
                parameters.fov_vd_max = ui->vd_max->text().toDouble();
                parameters.fov_vd_min = ui->vd_min->text().toDouble();
                parameters.cam_oft_x = ui->cam_x->text().toDouble();
                parameters.cam_oft_y = ui->cam_y->text().toDouble();
                parameters.cam_oft_z = ui->cam_z->text().toDouble();
                parameters.resolutoin_x = ui->rsl_x->text().toDouble();
                parameters.resolution_y= ui->rsl_y->text().toDouble();
                parameters.OVD = ui->ovd->text().toDouble();
                parameters.VD = ui->ovd->text().toDouble();
                parameters.vd_far = ui->vd_far->text().toDouble();
                 parameters.slant = ui->slant_ini->text().toDouble();
                 parameters.hcp = ui->hcp_ini->text().toDouble();
                 parameters.x_off_lens = ui->xoff_lens_ini->text().toDouble();
                 parameters.WS_OVD = ui->ws_ovd_ini->text().toDouble();
                 parameters.A = ui->a_ini->text().toDouble();
                 parameters.B = ui->b_ini->text().toDouble();
                 parameters.C = ui->c_ini->text().toDouble();
                 parameters.E1 = ui->e1_ini->text().toDouble();
                 parameters.E2 = ui->e2_ini->text().toDouble();
                 //parameters.model_type = ui->model_type->text().toInt();
                 parameters.model_type=1;
                 parameters.module_name=ui->module_type->text().toStdString();
                 parameters.panel_id = ui->panel_id->text().toStdString();

                 //initial calibrate parameters
                 my_calibrate.hcp = parameters.hcp;
                 my_calibrate.slant = parameters.slant;
                 my_calibrate.ws_ovd = parameters.WS_OVD;
                 my_calibrate.xoff_lens = parameters.x_off_lens;
                 my_calibrate.view_zone = 0;
                 my_calibrate.a = parameters.A;
                 my_calibrate.b = parameters.B;
                 my_calibrate.c = parameters.C;
                 my_calibrate.e1 = parameters.E1;
                 my_calibrate.e2 = parameters.E2;
                 my_calibrate.cam_off_x=parameters.cam_oft_x;
                 my_calibrate.cam_off_y = parameters.cam_oft_y;
                 my_calibrate.cam_off_vd = parameters.cam_oft_z;

                 my_hcp_slant.slant_adjust_value = ui->slant_adjust_ini->text().toDouble();
                 my_hcp_slant.hcp_adjust_value = ui->hcp_adjust_ini->text().toDouble();
                 my_xoff_lens.Xoff_adjust_value = ui->xoff_adjust_step->text().toDouble();
                 my_ws_ovd.ws_ovd_adjust_step = ui->ws_ovd_adjust_step->text().toDouble();
                 my_view_zone.view_adjust_step = ui->view_zone_step->text().toDouble();
                 my_vd_it.vd_adjust_step = ui->vd_step->text().toDouble();
                 my_vd_it.it_adjust_step = ui->it_step->text().toDouble();


                //WRITE PARAMETERS
                 if(calibrate_mode ==1){
                     uint8_t* tmp_uns_data = (uint8_t*)malloc(sizeof(uint8_t) * 1);
                     *tmp_uns_data = 1;
                      AUO3D_SendData(PanelData::CALIBRATION, tmp_uns_data);
                      AUO3D_SendData(PanelData::FOV_H_MAX, &parameters.fov_h_max );
                      AUO3D_SendData(PanelData::FOV_H_MIN, &parameters.fov_h_min);
                      AUO3D_SendData(PanelData::FOV_V_MIN, &parameters.fov_v_min);
                      AUO3D_SendData(PanelData::FOV_V_MAX, &parameters.fov_v_max);
                      AUO3D_SendData(PanelData::FOV_VD_MAX, &parameters.fov_vd_max);
                      AUO3D_SendData(PanelData::FOV_VD_MIN, &parameters.fov_vd_min);
                      AUO3D_SendData(PanelData::CAM_OFT_X, &parameters.cam_oft_x);
                      AUO3D_SendData(PanelData::CAM_OFT_Y, &parameters.cam_oft_y);
                      AUO3D_SendData(PanelData::CAM_OFT_Z, &parameters.cam_oft_z);
                      AUO3D_SendData(PanelData::RESOLUTION_X, &parameters.resolutoin_x);
                      AUO3D_SendData(PanelData::RESOLUTION_Y, &parameters.resolution_y);
                      AUO3D_SendData(PanelData::OVD, &parameters.OVD);
                      AUO3D_SendData(PanelData::VD, &parameters.OVD);
                      AUO3D_SendData(PanelData::SLANT, &parameters.slant);
                      AUO3D_SendData(PanelData::HCP, &parameters.hcp);
                      AUO3D_SendData(PanelData::XOFF_LENS, &parameters.x_off_lens);
                      double WS_OVD = 0;
                      AUO3D_SendData(PanelData::WS_OVD, &WS_OVD);
                      AUO3D_SendData(PanelData::A, &parameters.A);
                      AUO3D_SendData(PanelData::B, &parameters.B);
                      AUO3D_SendData(PanelData::C, &parameters.C);
                      AUO3D_SendData(PanelData::E1, &parameters.E1);
                      AUO3D_SendData(PanelData::E2, &parameters.E2);
                      AUO3D_SendData(PanelData::X, &parameters.X);
                      AUO3D_SendData(PanelData::Y, &parameters.Y);
                 }else if(calibrate_mode==0){
                     AUO3D_FPGA_SendData(PanelData::FOV_H_MAX, &parameters.fov_h_max );
                     AUO3D_FPGA_SendData(PanelData::FOV_H_MIN, &parameters.fov_h_min);
                     AUO3D_FPGA_SendData(PanelData::FOV_V_MIN, &parameters.fov_v_min);
                     AUO3D_FPGA_SendData(PanelData::FOV_V_MAX, &parameters.fov_v_max);
                     AUO3D_FPGA_SendData(PanelData::FOV_VD_MAX, &parameters.fov_vd_max);
                     AUO3D_FPGA_SendData(PanelData::FOV_VD_MIN, &parameters.fov_vd_min);
                     AUO3D_FPGA_SendData(PanelData::CAM_OFT_X, &parameters.cam_oft_x);
                     AUO3D_FPGA_SendData(PanelData::CAM_OFT_Y, &parameters.cam_oft_y);
                     AUO3D_FPGA_SendData(PanelData::CAM_OFT_Z, &parameters.cam_oft_z);
                     AUO3D_FPGA_SendData(PanelData::RESOLUTION_X, &parameters.resolutoin_x);
                     AUO3D_FPGA_SendData(PanelData::RESOLUTION_Y, &parameters.resolution_y);
                     AUO3D_FPGA_SendData(PanelData::OVD, &parameters.OVD);
                     AUO3D_FPGA_SendData(PanelData::VD, &parameters.VD);
                     AUO3D_FPGA_SendData(PanelData::SLANT, &parameters.slant);
                     AUO3D_FPGA_SendData(PanelData::HCP, &parameters.hcp);
                     AUO3D_FPGA_SendData(PanelData::XOFF_LENS, &parameters.x_off_lens);
                     AUO3D_FPGA_SendData(PanelData::WS_OVD, &parameters.WS_OVD);
                     AUO3D_FPGA_SendData(PanelData::X, &parameters.X);
                     AUO3D_FPGA_SendData(PanelData::Y, &parameters.Y);
                     AUO3D_FPGA_SendData(PanelData::A, &parameters.A);
                     AUO3D_FPGA_SendData(PanelData::B, &parameters.B);
                     AUO3D_FPGA_SendData(PanelData::C, &parameters.C);
                     AUO3D_FPGA_SendData(PanelData::E1, &parameters.E1);
                     AUO3D_FPGA_SendData(PanelData::E2, &parameters.E2);
                     AUO3D_FPGA_SendData(PanelData::X, &parameters.X);
                     AUO3D_FPGA_SendData(PanelData::Y, &parameters.Y);
                 }

                 return 1;
            }

        }







}

//--------------------------------------------------- UI
QPixmap MainWindow::load_image(QString path,int width,int height){
    QPixmap img(path);
    img = img.scaled(width,height);
    return img;
}

void MainWindow::Second_Monitor_mode(int mode) {
    QPixmap img;
    QString qstr = QString::fromStdString(pattern_name[mode]);
    img.load(qstr);
    img = img.scaled(3840, 2160);
    second_monitor.label->setPixmap(img);
    if (!second_monitor.widget->isVisible()) {
        second_monitor.widget->show();
    }
}

void MainWindow::preview_camera(){
     Mat rgb_frame;
     QImage display;
    //camera setting
     if(current_page==2){
        if(!single.current_frame.empty()){
            cvtColor(single.current_frame,rgb_frame, cv::COLOR_BGR2RGB);
            cv::resize(rgb_frame, rgb_frame, Size(ui->single_preview->width(), ui->single_preview->height()), 0, 0, INTER_CUBIC);
            display = QImage((uchar*)(rgb_frame.data), rgb_frame.cols, rgb_frame.rows, rgb_frame.step, QImage::Format_RGB888);
            ui->single_preview ->setPixmap(QPixmap::fromImage(display));
        }
        if(!dual_1.current_frame.empty()){
            cvtColor(dual_1.current_frame,rgb_frame, cv::COLOR_BGR2RGB);
            cv::resize(rgb_frame, rgb_frame, Size(ui->dual_1_preview->width(), ui->dual_1_preview->height()), 0, 0, INTER_CUBIC);
            display = QImage((uchar*)(rgb_frame.data), rgb_frame.cols, rgb_frame.rows, rgb_frame.step, QImage::Format_RGB888);
            ui->dual_1_preview ->setPixmap(QPixmap::fromImage(display));
        }
        if(!dual_2.current_frame.empty()){
            cvtColor(dual_2.current_frame,rgb_frame, cv::COLOR_BGR2RGB);
            cv::resize(rgb_frame, rgb_frame, Size(ui->dual_2_preview->width(), ui->dual_2_preview->height()), 0, 0, INTER_CUBIC);
            display = QImage((uchar*)(rgb_frame.data), rgb_frame.cols, rgb_frame.rows, rgb_frame.step, QImage::Format_RGB888);
            ui->dual_2_preview ->setPixmap(QPixmap::fromImage(display));
        }
    }
    //hcp slant
    else if(current_page==3){

        if(!single.current_frame.empty()){
            cvtColor(single.current_frame,rgb_frame, cv::COLOR_BGR2RGB);
            cv::resize(rgb_frame, rgb_frame, Size(ui->hcp_preview->width(), ui->hcp_preview->height()), 0, 0, INTER_CUBIC);
            display = QImage((uchar*)(rgb_frame.data), rgb_frame.cols, rgb_frame.rows, rgb_frame.step, QImage::Format_RGB888);
            ui->hcp_preview ->setPixmap(QPixmap::fromImage(display));
        }
    //xoff lens
    }
     //xoff_lens
     else if(current_page==4){
        if(!single.current_frame.empty()){
            Point center(single.current_frame.cols/2, single.current_frame.rows/2);
            Point Top(single.current_frame.cols/2, 0);
            Point Left(0, single.current_frame.rows/2);
            cvtColor(single.current_frame,rgb_frame, cv::COLOR_BGR2RGB);
            line(rgb_frame, center, Top, Scalar(255, 0, 0), 12, LINE_8);
            line(rgb_frame, center, Left, Scalar(255, 0, 0), 12, LINE_8);
            cv::resize(rgb_frame, rgb_frame, Size(ui->hcp_preview->width(), ui->hcp_preview->height()), 0, 0, INTER_CUBIC);
            display = QImage((uchar*)(rgb_frame.data), rgb_frame.cols, rgb_frame.rows, rgb_frame.step, QImage::Format_RGB888);
            ui->xoff_preview ->setPixmap(QPixmap::fromImage(display));
        }

    }
    //ws ovd
     else if(current_page==5){
         if(!single.current_frame.empty()){
             cvtColor(single.current_frame,rgb_frame, cv::COLOR_BGR2RGB);
             cv::resize(rgb_frame, rgb_frame, Size(ui->hcp_preview->width(), ui->hcp_preview->height()), 0, 0, INTER_CUBIC);
             display = QImage((uchar*)(rgb_frame.data), rgb_frame.cols, rgb_frame.rows, rgb_frame.step, QImage::Format_RGB888);
             ui->ws_ovd_preview ->setPixmap(QPixmap::fromImage(display));
         }
     }
     //view zone
     else if(current_page==6){
         if(!dual_1.current_frame.empty()){

             cvtColor(dual_1.current_frame,rgb_frame, cv::COLOR_BGR2RGB);
             if(dual_1.is_contour){
                 cv::rectangle(rgb_frame, dual_1.bounding_rect, cv::Scalar(0, 255, 0), 10, 8, 0);
             }
             cv::resize(rgb_frame, rgb_frame, Size( ui->view_zone_preview1->width(),  ui->view_zone_preview1->height()), 0, 0, INTER_CUBIC);
             display = QImage((uchar*)(rgb_frame.data), rgb_frame.cols, rgb_frame.rows, rgb_frame.step, QImage::Format_RGB888);
             ui->view_zone_preview1 ->setPixmap(QPixmap::fromImage(display));

         }
         if(!dual_2.current_frame.empty()){

             cvtColor(dual_2.current_frame,rgb_frame, cv::COLOR_BGR2RGB);
             if(dual_2.is_contour){
                 cv::rectangle(rgb_frame, dual_2.bounding_rect, cv::Scalar(0, 255, 0), 10, 8, 0);
             }
             cv::resize(rgb_frame, rgb_frame, Size( ui->view_zone_preview2->width(),  ui->view_zone_preview2->height()), 0, 0, INTER_CUBIC);
             display = QImage((uchar*)(rgb_frame.data), rgb_frame.cols, rgb_frame.rows, rgb_frame.step, QImage::Format_RGB888);
             ui->view_zone_preview2 ->setPixmap(QPixmap::fromImage(display));
         }

     }

     else if(current_page==7){
         if(!dual_1.current_frame.empty()){

             cvtColor(dual_1.current_frame,rgb_frame, cv::COLOR_BGR2RGB);
             if(dual_1.is_contour){
                 cv::rectangle(rgb_frame, dual_1.bounding_rect, cv::Scalar(0, 255, 0), 10, 8, 0);
             }
             cv::resize(rgb_frame, rgb_frame, Size( ui->vd_preview_1->width(),  ui->vd_preview_1->height()), 0, 0, INTER_CUBIC);
             display = QImage((uchar*)(rgb_frame.data), rgb_frame.cols, rgb_frame.rows, rgb_frame.step, QImage::Format_RGB888);
             ui->vd_preview_1 ->setPixmap(QPixmap::fromImage(display));

         }
         if(!dual_2.current_frame.empty()){

             cvtColor(dual_2.current_frame,rgb_frame, cv::COLOR_BGR2RGB);
             if(dual_2.is_contour){
                 cv::rectangle(rgb_frame, dual_2.bounding_rect, cv::Scalar(0, 255, 0), 10, 8, 0);
             }
             cv::resize(rgb_frame, rgb_frame, Size( ui->vd_preview_2->width(),  ui->vd_preview_2->height()), 0, 0, INTER_CUBIC);
             display = QImage((uchar*)(rgb_frame.data), rgb_frame.cols, rgb_frame.rows, rgb_frame.step, QImage::Format_RGB888);
             ui->vd_preview_2 ->setPixmap(QPixmap::fromImage(display));
         }



     }
     else if(current_page==8){
         if(!dual_1.current_frame.empty()){

             cvtColor(dual_1.current_frame,rgb_frame, cv::COLOR_BGR2RGB);
             if(dual_1.is_contour){
                 cv::rectangle(rgb_frame, dual_1.bounding_rect, cv::Scalar(0, 255, 0), 10, 8, 0);
             }
             cv::resize(rgb_frame, rgb_frame, Size( ui->cam_1_check->width(),  ui->cam_1_check->height()), 0, 0, INTER_CUBIC);
             display = QImage((uchar*)(rgb_frame.data), rgb_frame.cols, rgb_frame.rows, rgb_frame.step, QImage::Format_RGB888);
             ui->cam_1_check ->setPixmap(QPixmap::fromImage(display));

         }
         if(!dual_2.current_frame.empty()){

             cvtColor(dual_2.current_frame,rgb_frame, cv::COLOR_BGR2RGB);
             if(dual_2.is_contour){
                 cv::rectangle(rgb_frame, dual_2.bounding_rect, cv::Scalar(0, 255, 0), 10, 8, 0);
             }
             cv::resize(rgb_frame, rgb_frame, Size( ui->cam_2_check->width(),  ui->cam_2_check->height()), 0, 0, INTER_CUBIC);
             display = QImage((uchar*)(rgb_frame.data), rgb_frame.cols, rgb_frame.rows, rgb_frame.step, QImage::Format_RGB888);
             ui->cam_2_check ->setPixmap(QPixmap::fromImage(display));
         }

     }

}

//--------------------------------------------------- Page Control
void MainWindow::page_add(){
    current_page++ ;
    animationStackedWidgets();
    ui->stackedWidget->setCurrentIndex(current_page);

    if(current_page==1){
       // ui->start_calibration->setEnabled(false);
        ui->back->show();
        if(calibrate_mode==0){
            QPixmap img = load_image("./FPGA.png",ui->mode_image->width(),ui->mode_image->height());
            ui->mode_name->setText("FPGA mode");
            ui->mode_image->setPixmap(img);
            ui->start_calibration->setText(QString::fromLocal8Bit("請先連線"));
        }else if(calibrate_mode==1){
            QPixmap img = load_image("./PC_clear.png",ui->mode_image->width(),ui->mode_image->height());
            ui->start_calibration->setText(QString::fromLocal8Bit("開始校正"));
            ui->mode_name->setText("PC mode");
            ui->mode_image->setPixmap(img);
            ui->start_calibration->setEnabled(true);
       }
    }
    else if(current_page==2){
        ui->Calibration_Function_Box->show();
        ui->back->hide();
        if(single.id==-1){
            check_camera_device();
        }

    }




}

void MainWindow::page_dec(){
    current_page-- ;
    animationStackedWidgets();
    ui->stackedWidget->setCurrentIndex(current_page);

    if(current_page==0){
        ui->start_calibration->setEnabled(false);
        ui->back->hide();
    }
}

void MainWindow::to_next_calibrate_step(){
    if(current_page==3){
        to_xoff_len_page();
       // ui->calibrate_next->hide();
    }else if(current_page==4){
        to_view_zone_test_page();
        //to_ws_ovd_page();
       // ui->calibrate_next->hide();
    }else if(current_page==5){
        to_view_zone_test_page();
       // ui->calibrate_next->hide();
    }
    else if(current_page==6){
        to_vd_it_page();
       // ui->calibrate_next->hide();
    }
    else if(current_page==7){
        to_finish_page();
       // ui->calibrate_next->hide();
    }
    else if(current_page==9){
        ui->end_calibration->show();
        second_monitor.widget->hide();
       // ui->calibrate_next->hide();
        to_hcp_slant_page();
    }
    if(current_page!=9){
        ui->end_calibration->hide();
    }


}

void MainWindow::to_env_setting_page(){
    current_page=9;
    ui->calibrate_next->show();
    animationStackedWidgets();
    //ui->calibrate_next->show();
    ui->stackedWidget->setCurrentIndex(current_page);
    if(calibrate_mode==1){
        Second_Monitor_mode(C);
    }else if(calibrate_mode==0){
        Second_Monitor_mode(C_C);
    }

}

void MainWindow::to_camera_setting_page(){
    current_page = 2;
    animationStackedWidgets();
    ui->calibrate_next->hide();
    ui->stackedWidget->setCurrentIndex(current_page);
/*
    if(fpga_connect==false){
        fpga_ip_connect();
        ui->fpga_status->setStyleSheet("QLabel { color : white; }");
        ui->fpga_status->setText(QString::fromLocal8Bit("FPGA 校正模組:: 已連線"));
        if(calibrate_mode ==1){
            uint8_t* tmp_uns_data = (uint8_t*)malloc(sizeof(uint8_t) * 1);
            *tmp_uns_data = 1;
            AUO3D_SendData(PanelData::FOV_H_MAX, &parameters.fov_h_max );
            AUO3D_SendData(PanelData::FOV_H_MIN, &parameters.fov_h_min);
            AUO3D_SendData(PanelData::FOV_V_MIN, &parameters.fov_v_min);
            AUO3D_SendData(PanelData::FOV_V_MAX, &parameters.fov_v_max);
            AUO3D_SendData(PanelData::FOV_VD_MAX, &parameters.fov_vd_max);
            AUO3D_SendData(PanelData::FOV_VD_MIN, &parameters.fov_vd_min);
            AUO3D_SendData(PanelData::CAM_OFT_X, &my_calibrate.cam_off_x);
            AUO3D_SendData(PanelData::CAM_OFT_Y, &my_calibrate.cam_off_y);
            AUO3D_SendData(PanelData::CAM_OFT_Z, &my_calibrate.cam_off_vd);
            AUO3D_SendData(PanelData::RESOLUTION_X, &parameters.resolutoin_x);
            AUO3D_SendData(PanelData::RESOLUTION_Y, &parameters.resolution_y);
            AUO3D_SendData(PanelData::OVD, &parameters.OVD);
            AUO3D_SendData(PanelData::VD, &parameters.VD);
            AUO3D_SendData(PanelData::SLANT, &my_calibrate.slant);
            AUO3D_SendData(PanelData::HCP, &my_calibrate.hcp);
            AUO3D_SendData(PanelData::XOFF_LENS, &my_calibrate.xoff_lens);
            AUO3D_SendData(PanelData::WS_OVD,&my_calibrate.ws_ovd);
            AUO3D_SendData(PanelData::X, &parameters.X);
            AUO3D_SendData(PanelData::Y, &parameters.Y);
            AUO3D_SendData(PanelData::A, &my_calibrate.a);
            AUO3D_SendData(PanelData::B, &my_calibrate.b);
            AUO3D_SendData(PanelData::C, &my_calibrate.c);
            AUO3D_SendData(PanelData::E1, &my_calibrate.e1);
            AUO3D_SendData(PanelData::E2, &my_calibrate.e2);
        }else if(calibrate_mode==0){
            AUO3D_FPGA_SendData(PanelData::FOV_H_MAX, &parameters.fov_h_max );
            AUO3D_FPGA_SendData(PanelData::FOV_H_MIN, &parameters.fov_h_min);
            AUO3D_FPGA_SendData(PanelData::FOV_V_MIN, &parameters.fov_v_min);
            AUO3D_FPGA_SendData(PanelData::FOV_V_MAX, &parameters.fov_v_max);
            AUO3D_FPGA_SendData(PanelData::FOV_VD_MAX, &parameters.fov_vd_max);
            AUO3D_FPGA_SendData(PanelData::FOV_VD_MIN, &parameters.fov_vd_min);
            AUO3D_FPGA_SendData(PanelData::CAM_OFT_X, &my_calibrate.cam_off_x);
            AUO3D_FPGA_SendData(PanelData::CAM_OFT_Y, &my_calibrate.cam_off_y);
            AUO3D_FPGA_SendData(PanelData::CAM_OFT_Z, &my_calibrate.cam_off_vd);
            AUO3D_FPGA_SendData(PanelData::RESOLUTION_X, &parameters.resolutoin_x);
            AUO3D_FPGA_SendData(PanelData::RESOLUTION_Y, &parameters.resolution_y);
            AUO3D_FPGA_SendData(PanelData::OVD, &parameters.OVD);
            AUO3D_FPGA_SendData(PanelData::VD, &parameters.VD);
            AUO3D_FPGA_SendData(PanelData::SLANT, &my_calibrate.slant);
            AUO3D_FPGA_SendData(PanelData::HCP, &my_calibrate.hcp);
            AUO3D_FPGA_SendData(PanelData::XOFF_LENS, &my_calibrate.xoff_lens);
            AUO3D_FPGA_SendData(PanelData::WS_OVD,&my_calibrate.ws_ovd);
            AUO3D_FPGA_SendData(PanelData::X, &parameters.X);
            AUO3D_FPGA_SendData(PanelData::Y, &parameters.Y);
            AUO3D_FPGA_SendData(PanelData::A, &my_calibrate.a);
            AUO3D_FPGA_SendData(PanelData::B, &my_calibrate.b);
            AUO3D_FPGA_SendData(PanelData::C, &my_calibrate.c);
            AUO3D_FPGA_SendData(PanelData::E1, &my_calibrate.e1);
            AUO3D_FPGA_SendData(PanelData::E2, &my_calibrate.e2);
        }
        eye_tracking_thread_start();
        Sleep(300);
        check_FPGA_status();


    }
*/

}

void MainWindow::to_hcp_slant_page(){
    if(current_page!=3){
        current_page = 3;
        //ui->calibrate_next->show();
        animationStackedWidgets();
        ui->stackedWidget->setCurrentIndex(current_page);
        ui->camera_setting->setEnabled(true);

        double vd=parameters.OVD;

        if(calibrate_mode==1){
            AUO3D_SendData(PanelData::VD, &vd);
             AUO3D_imagePath_LR("./RED.bmp","./GREEN.bmp");

        }else if(calibrate_mode==0){
            AUO3D_FPGA_SendData(PanelData::VD, &vd);
            Second_Monitor_mode(R_G);
        }
    }
}

void MainWindow::to_xoff_len_page(){
    if(current_page!=4){
        current_page= 4;
        animationStackedWidgets();
        ui->stackedWidget->setCurrentIndex(current_page);
        if(calibrate_mode==1){
            AUO3D_imagePath_LR("./WHITE.bmp","./WHITE.bmp");
        }else if(calibrate_mode==0){
            Second_Monitor_mode(W);
        }
    }
}

void MainWindow::to_ws_ovd_page(){
    if(current_page!=5){

        current_page = 5;
        animationStackedWidgets();
        ui->stackedWidget->setCurrentIndex(current_page);
        if(calibrate_mode==1){
           AUO3D_SendData(PanelData::WS_OVD, &parameters.WS_OVD);
           double VD=parameters.vd_far;
            AUO3D_SendData(PanelData::VD, &VD);
             AUO3D_imagePath_LR("./RED.bmp","./GREEN.bmp");

        }else if(calibrate_mode==0){
            AUO3D_FPGA_SendData(PanelData::WS_OVD, &parameters.WS_OVD);
            double VD=parameters.vd_far;
            AUO3D_SendData(PanelData::VD, &VD);
            Second_Monitor_mode(R_G);
        }
    }

}

void MainWindow::to_view_zone_test_page(){
    if(current_page!=6){

        current_page = 6;
        animationStackedWidgets();
        ui->stackedWidget->setCurrentIndex(current_page);
        if(!dual_1.is_roi_open&&!dual_2.is_roi_open){
            open_roi_thread_1();
            open_roi_thread_2();
            dual_1.is_roi_open = true;
            dual_2.is_roi_open = true;
        }
        if(calibrate_mode==1){
            double VD=parameters.OVD;
             AUO3D_SendData(PanelData::VD, &VD);
             AUO3D_imagePath_LR("./RED.bmp","./GREEN.bmp");
        }else if(calibrate_mode==0){
            double VD=parameters.OVD;
            AUO3D_FPGA_SendData(PanelData::VD, &VD);
            Second_Monitor_mode(R_G);
        }

        //ui->calibrate_next->hide();
    }

}

void MainWindow::to_vd_it_page(){
    if(current_page!=7){

        current_page = 7;
        vd_id_stage();
        Eye_Tracking_timer->start();
        animationStackedWidgets();
        ui->stackedWidget->setCurrentIndex(current_page);

        if(!dual_1.is_roi_open&&!dual_2.is_roi_open){
            open_roi_thread_1();
            open_roi_thread_2();
            dual_1.is_roi_open = true;
            dual_2.is_roi_open = true;
        }
        if(calibrate_mode==1){
            double VD=parameters.OVD;
             AUO3D_SendData(PanelData::VD, &VD);
            AUO3D_imagePath_LR("./RED.bmp","./GREEN.bmp");
        }else if(calibrate_mode==0){
            double VD=parameters.OVD;
            AUO3D_FPGA_SendData(PanelData::VD, &VD);
            Second_Monitor_mode(R_G);
        }
    }


/*
    dual_1.side=1;
    my_vd_it.current_stage=3;
     connect(calibrate_timer, SIGNAL(timeout()),  vd_it_step, SLOT(map()));
    double X = 0.36396471 * (parameters.OVD+parameters.vd_far)/2;
    double VD = (parameters.OVD+parameters.vd_far)/2;


    if(calibrate_mode==0){
        AUO3D_FPGA_SendData(PanelData::X,&X);
        AUO3D_FPGA_SendData(PanelData::VD,&VD);

    }else if(calibrate_mode==1){
        AUO3D_SendData(PanelData::X,&X);
        AUO3D_SendData(PanelData::VD,&VD);
    }
*/


}

void MainWindow::to_finish_page(){
    current_page = 8;
    animationStackedWidgets();
    ui->Calibration_Function_Box->hide();
    ui->stackedWidget->setCurrentIndex(current_page);
    ui->final_panel_id->setText("Panel_ID::  "+QString::fromStdString(parameters.panel_id));
    ui->final_module_name->setText("Module_Name:  "+QString::fromStdString(parameters.module_name));
    ui->final_hcp->setText("HCP:  "+QString::number(my_calibrate.hcp));
    ui->final_slant->setText("Slant:  "+QString::number(my_calibrate.slant));
    ui->final_xoff->setText("Xoff Lens:  "+QString::number(my_calibrate.xoff_lens));
    ui->final_ws->setText("Window Size(OVD):  "+QString::number(my_calibrate.ws_ovd));
    ui->final_a->setText("VD_A:  "+QString::number(my_calibrate.a));
    ui->final_b->setText("VD_B:  "+QString::number(my_calibrate.b));
    ui->final_c->setText("VD_C:  "+QString::number(my_calibrate.c));
    ui->final_e1->setText("IT E1(>20。):  "+QString::number(my_calibrate.e1));
    ui->final_e2->setText("IT E2(<-20。):  "+QString::number(my_calibrate.e2));
    ui->final_view_zone->setText("View_Zone:  "+QString::number(my_calibrate.view_zone));
    ui->cam_off_x->setText("Camera Offset X:  "+QString::number(my_calibrate.cam_off_x));
    ui->cam_off_y->setText("Camera Offset Y:  "+QString::number(my_calibrate.cam_off_y));

    QDate currentDate = QDate::currentDate();
    ui->output_date->setText("Date:  "+ currentDate.toString() );
    ui->calibrate_next->hide();
    if(calibrate_mode==0){
        uint8_t* tmp_uns_data = (uint8_t*)malloc(sizeof(uint8_t) * 1);
        *tmp_uns_data = 0;
        AUO3D_FPGA_SendData(PanelData::CALIBRATION, tmp_uns_data);
    }else if(calibrate_mode==1){
        uint8_t* tmp_uns_data = (uint8_t*)malloc(sizeof(uint8_t) * 1);
        *tmp_uns_data = 0;
        AUO3D_SendData(PanelData::CALIBRATION, tmp_uns_data);
         // AUO3D_imagePath_LR("./3.bmp","./4.bmp");

    }

    if(dual_1.side==1){
        ui->check_tag_1->setText(QString::fromLocal8Bit("右眼"));
        ui->check_tag_2->setText(QString::fromLocal8Bit("左眼"));
    }else{
        ui->check_tag_1->setText(QString::fromLocal8Bit("左眼"));
        ui->check_tag_2->setText(QString::fromLocal8Bit("右眼"));
    }




}
//Switch Animation
void MainWindow::animationStackedWidgets()
{
    QGraphicsOpacityEffect *effect = new QGraphicsOpacityEffect(this);
    ui->stackedWidget->setGraphicsEffect(effect);
    QPropertyAnimation *anim = new QPropertyAnimation(effect,"opacity");
    anim->setDuration(250);
    anim->setStartValue(0);
    anim->setEndValue(1);
    anim->setEasingCurve(QEasingCurve::InBack);
    anim->start(QPropertyAnimation::DeleteWhenStopped);
    connect(anim, SIGNAL(finished()), this, SLOT(whenAnimationFinish()));

}

void MainWindow::whenAnimationFinish()
{
    ui->stackedWidget->setGraphicsEffect(0); // remove effect
}

//---------------------------------------------------Page 0 Function (select mode)
void MainWindow::select_FPGA(){
    calibrate_mode = 0;
    page_add();
    ui->IP_address_bar->show();
    ui->IP_connect->show();

}

void MainWindow::select_PC(){
    calibrate_mode = 1;
    page_add();
    ui->IP_address_bar->hide();
    ui->IP_connect->hide();
}

//---------------------------------------------------Page 1 Function (connect lib)
void MainWindow::start_calibrate(){
    int res;
    if(calibrate_mode==0){
        //FPGA base


        if(load_parameters()==1){
            Sleep(300);
            eye_tracking_thread_start();
            ui->current_mode->setText(QString::fromLocal8Bit("當前校正模式: FPGA"));
            ui->fpga_status->show();
            ui->fpga_status->setText(QString::fromLocal8Bit("FPGA 校正模組: 已連線"));
            check_FPGA_status();
            Second_Monitor_mode(R_G);
            page_add();
            ui->A_value->setText(QString::number(parameters.A));
            ui->B_value->setText(QString::number(parameters.B));
            ui->C_value->setText(QString::number(parameters.C));
            ui->E1_value->setText(QString::number(parameters.E1));
            ui->E2_value->setText(QString::number(parameters.E2));
        } else{
            return;
        }


    }
    else if(calibrate_mode==1){
        //PC base
        //AUO3D_init(resolution.width(),resolution.height());
        if(load_parameters()==1){
            int rsl_x = parameters.resolutoin_x;
            int rsl_y = parameters.resolution_y;
            AUO3D_init(rsl_x,rsl_y);
            //dont need write emmc
            ui->load_to_emmc->hide();
            ui->to_emmc->hide();
            Sleep(100);
            if(AUO3D_isRunning()){
                ui->current_mode->setText(QString::fromLocal8Bit("當前校正模式: PC"));
                ui->pc_status->show();
                ui->pc_status->setText(QString::fromLocal8Bit("PC 校正模組: 已連線"));
                check_PC_status();
                page_add();
                ui->A_value->setText(QString::number(parameters.A));
                ui->B_value->setText(QString::number(parameters.B));
                ui->C_value->setText(QString::number(parameters.C));
                ui->E1_value->setText(QString::number(parameters.E1));
                ui->E2_value->setText(QString::number(parameters.E2));
                pc_eye_tracking_thread_start();

            }

        }else{
            return;
        }


    }

}

void MainWindow::fpga_ip_connect(){
    if(! ui->ip_content->text().isEmpty()){
        clock_t call_start, call_time;
        QByteArray byteArray = ui->ip_content->text().toUtf8();
        char* ip = byteArray.data();
        ui->fpga_connect->setEnabled(false);
        ui->IP_connect->setStyleSheet("QLabel { color : green; }");
        ui->IP_connect->setText(QString::fromLocal8Bit("連線狀態: 連線中...."));
        call_start = clock();
        while(1){
            int res = AUO3D_FPGA_Init(ip, strlen(ip));
            if(res){
                ui->IP_connect->setStyleSheet("QLabel { color : green; }");
                ui->IP_connect->setText(QString::fromLocal8Bit("連線狀態: 連線成功"));
                ui->start_calibration->setEnabled(true);
                ui->start_calibration->setText(QString::fromLocal8Bit("開始校正"));
                uint8_t* tmp_uns_data = (uint8_t*)malloc(sizeof(uint8_t) * 1);
                *tmp_uns_data = 1;
                res = AUO3D_FPGA_SendData(PanelData::CALIBRATION, tmp_uns_data);
                fpga_connect=true;
                break;
            }
            call_time = clock();
            if(double(call_time) - double(call_start) > 5000){
                ui->IP_connect->setStyleSheet("QLabel { color : red; }");
                ui->IP_connect->setText(QString::fromLocal8Bit("連線狀態: 連線失敗"));
                ui->fpga_connect->setEnabled(true);
                break;
            }
            Sleep(100);
        }
    }
}

void MainWindow::load_ini_file(){

    QString filePath = QFileDialog::getOpenFileName(nullptr, QString::fromLocal8Bit("选择文本文件"), "",QString::fromLocal8Bit( "文本文件 (*.ini)"));
    if (filePath.isEmpty()) {
        return;
    }


    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qDebug() << "can not open file：" << file.errorString();
        return;
    }

    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine();
        QStringList stringList = line.split("=");

        if(stringList.at(0)=="CAM_OFT_X"){
            ui->cam_x->setText(stringList.at(1));
        }else if(stringList.at(0)=="CAM_OFT_Y"){
            ui->cam_y->setText(stringList.at(1));
        }else if(stringList.at(0)=="CAM_OFT_Z"){
             ui->cam_z->setText(stringList.at(1));
        }else if (stringList.at(0)=="FOV_H_min"){
             ui->h_min->setText(stringList.at(1));
        }else if(stringList.at(0)=="FOV_H_max"){
              ui->h_max->setText(stringList.at(1));
        }else if(stringList.at(0)=="FOV_V_max"){
               ui->v_max->setText(stringList.at(1));
        }else if(stringList.at(0)=="FOV_V_min"){
               ui->v_min->setText(stringList.at(1));
        }else if(stringList.at(0)=="FOV_VD_min"){
               ui->vd_min->setText(stringList.at(1));
        }else if(stringList.at(0)=="FOV_VD_max"){
               ui->vd_max->setText(stringList.at(1));
        }else if(stringList.at(0)=="resolutoin_x"){
            ui->rsl_x->setText(stringList.at(1));
        }else if(stringList.at(0)=="resolutoin_y"){
            ui->rsl_y->setText(stringList.at(1));
        }else if(stringList.at(0)=="OVD"){
               ui->ovd->setText(stringList.at(1));
        }else if(stringList.at(0)=="VD_FAR"){
            ui->vd_far->setText(stringList.at(1));
        }else if(stringList.at(0)=="Slant"){
               ui->slant_ini->setText(stringList.at(1));
        }else if(stringList.at(0)=="HCP"){
               ui->hcp_ini->setText(stringList.at(1));
        }else if(stringList.at(0)=="Xoff_Lens"){
               ui->xoff_lens_ini->setText(stringList.at(1));
        }else if(stringList.at(0)=="WS_OVD"){
               ui->ws_ovd_ini->setText(stringList.at(1));
        }else if(stringList.at(0)=="a"){
               ui->a_ini->setText(stringList.at(1));
        }else if(stringList.at(0)=="b"){
               ui->b_ini->setText(stringList.at(1));
        }else if(stringList.at(0)=="c"){
               ui->c_ini->setText(stringList.at(1));
        }else if(stringList.at(0)=="E1"){
               ui->e1_ini->setText(stringList.at(1));
        }else if(stringList.at(0)=="E2"){
               ui->e2_ini->setText(stringList.at(1));
        }else if(stringList.at(0)=="hcp_adjust_value"){
               ui->hcp_adjust_ini->setText(stringList.at(1));
        }else if(stringList.at(0)=="slant_adjust_value"){
               ui->slant_adjust_ini->setText(stringList.at(1));
        }else if(stringList.at(0)=="Xoff_adjust_step"){
               ui->xoff_adjust_step->setText(stringList.at(1));
        }else if(stringList.at(0)=="WS_OVD_adjust_step"){
               ui->ws_ovd_adjust_step->setText(stringList.at(1));
        }else if(stringList.at(0)=="View_Zone_adjust_step"){
               ui->view_zone_step->setText(stringList.at(1));
        }else if(stringList.at(0)=="VD_adjust_step"){
               ui->vd_step->setText(stringList.at(1));
        }else if(stringList.at(0)=="IT_adjust_step"){
               ui->it_step->setText(stringList.at(1));
        }
        else if(stringList.at(0)=="Module_Type"){
            ui->module_type->setText(stringList.at(1));
        }else if(stringList.at(0)=="panel_id"){
            ui->panel_id->setText(stringList.at(1));
        }
    }
      file.close();


}

//---------------------------------------------------Page 2 Function (camera)
void MainWindow::reconnect_camera(){
    ui->Warning->setText("");
    camera_fresh_timer->stop();
    single.is_close= true;
    dual_1.is_close= true;
    dual_2.is_close = true;
    single.id = -1;
    dual_1.id = -1;
    dual_1.side =-1;
    dual_2.id = -1;
    dual_2.side = -1;
    Sleep(500);
    check_camera_device();

}

//---------------------------------------------------Page 3 Function (HCP / Slant)
void MainWindow::start_hcp(){

    file.open(QIODevice::WriteOnly | QIODevice::Text);

    ui->Calibration_Function_Box->setEnabled(false);
    connect(calibrate_timer, SIGNAL(timeout()),  hcp_slant_step, SLOT(map()));
    hcp_slant_step->setMapping(calibrate_timer, 0);
    
    my_hcp_slant.hcp = parameters.hcp;
    my_hcp_slant.slant = parameters.slant;
    double ws_ovd = 0;
    double xoff_lens = my_calibrate.xoff_lens;
    double a = 0;
    double b = 0;
    double c = 0;
    double e1 = 0;
    double e2 = 0;


    if(calibrate_mode==1){
        AUO3D_SendData(PanelData::HCP, &parameters.hcp);
        AUO3D_SendData(PanelData::SLANT, &parameters.slant);
        AUO3D_SendData(PanelData::WS_OVD, &ws_ovd);
        AUO3D_SendData(PanelData::XOFF_LENS, &xoff_lens);
        AUO3D_SendData(PanelData::A, &a);
        AUO3D_SendData(PanelData::B, &b);
        AUO3D_SendData(PanelData::C, &c);
        AUO3D_SendData(PanelData::E1, &e1);
        AUO3D_SendData(PanelData::E2, &e2);
        
         AUO3D_imagePath_LR("./RED.bmp","./RED.bmp");

    }else if(calibrate_mode==0){
        AUO3D_FPGA_SendData(PanelData::HCP, &parameters.hcp);
        AUO3D_FPGA_SendData(PanelData::SLANT, &parameters.slant);
        AUO3D_FPGA_SendData(PanelData::WS_OVD, &ws_ovd);
        AUO3D_FPGA_SendData(PanelData::XOFF_LENS, &xoff_lens);
        AUO3D_FPGA_SendData(PanelData::A, &a);
        AUO3D_FPGA_SendData(PanelData::B, &b);
        AUO3D_FPGA_SendData(PanelData::C, &c);
        AUO3D_FPGA_SendData(PanelData::E1, &e1);
        AUO3D_FPGA_SendData(PanelData::E2, &e2);
        Second_Monitor_mode(R_R);
    }
    calibrate_timer->start();
}

void MainWindow::calibrate_hcp(int step){
   if(step==0){
       int res;
        my_hcp_slant.is_calibrating = true;
                        
       /*
        //check to delete
       if(calibrate_mode==1){
           my_hcp_slant.hcp = parameters.hcp;
           my_hcp_slant.slant = parameters.slant;
           my_hcp_slant.ws_ovd = parameters.WS_OVD;
           if(calibrate_mode==1){
               res = AUO3D_SendData(PanelData::SLANT, &my_hcp_slant.slant);
               res = AUO3D_SendData(PanelData::HCP, &my_hcp_slant.hcp);
           }else if(calibrate_mode==0){
               res = AUO3D_FPGA_SendData(PanelData::SLANT, &my_hcp_slant.slant);
               res = AUO3D_FPGA_SendData(PanelData::HCP, &my_hcp_slant.hcp);
               res = AUO3D_FPGA_SendData(PanelData::WS_OVD, &my_hcp_slant.ws_ovd);
           }
           AUO3D_imagePath_LR(PC_image_path(R),PC_image_path(R));
       }
       else{
           my_hcp_slant.hcp = parameters.hcp;
           my_hcp_slant.slant = parameters.slant;
           my_hcp_slant.ws_ovd = parameters.WS_OVD;
           if(calibrate_mode==1){
               res = AUO3D_SendData(PanelData::SLANT, &my_hcp_slant.slant);
               res = AUO3D_SendData(PanelData::HCP, &my_hcp_slant.hcp);
           }else if(calibrate_mode==0){
               res = AUO3D_FPGA_SendData(PanelData::SLANT, &my_hcp_slant.slant);
               res = AUO3D_FPGA_SendData(PanelData::HCP, &my_hcp_slant.hcp);
               res = AUO3D_FPGA_SendData(PanelData::WS_OVD, &my_hcp_slant.ws_ovd);
           }
           Second_Monitor_mode(R_R);
       }
       */

       ui->start_calibrate_hcp->setText(QString::fromLocal8Bit("校正中..."));
       ui->start_calibrate_hcp->setEnabled(false);

       hcp_slant_step->setMapping(calibrate_timer, 1);
       calibrate_timer->start();
   }
    //ref red
   else if(step==1){     
       int* rgb = new int[3];
       int* hsv = new int[3];
       Mat tmp_frame = single.current_frame.clone();
       Calibrate.set_ref_by_camera(tmp_frame, 'R');
       rgb = Calibrate.get_ref_rgb('R');
       hsv = Calibrate.get_ref_hsv('R');

       Mat display_ref_r(ui->ref_red->height(), ui->ref_red->width(), CV_8UC3, Scalar(rgb[0], rgb[1], rgb[2]));
       QImage display = QImage((uchar*)(display_ref_r.data), display_ref_r.cols, display_ref_r.rows, display_ref_r.step, QImage::Format_RGB888);
       ui->ref_red->setPixmap(QPixmap::fromImage(display));

       my_hcp_slant.ref_red_h = hsv[0];
       my_hcp_slant.ref_red_s = hsv[1];
       if(calibrate_mode==1){
            AUO3D_imagePath_LR(PC_image_path(G),PC_image_path(G));
       }else if(calibrate_mode==0){
           Second_Monitor_mode(G_G);
       }

       hcp_slant_step->setMapping(calibrate_timer, 2);
       calibrate_timer->start();
   }
   //ref green
   else if(step==2){
       int* rgb = new int[3];
       int* hsv = new int[3];
       Mat tmp_frame = single.current_frame.clone();
       Calibrate.set_ref_by_camera(tmp_frame, 'G');
       rgb = Calibrate.get_ref_rgb('G');
       hsv = Calibrate.get_ref_hsv('G');

       Mat display_ref_g(ui->ref_green->height(), ui->ref_green->width(), CV_8UC3, Scalar(rgb[0], rgb[1], rgb[2]));
       QImage display = QImage((uchar*)(display_ref_g.data), display_ref_g.cols, display_ref_g.rows, display_ref_g.step, QImage::Format_RGB888);
       ui->ref_green->setPixmap(QPixmap::fromImage(display));

       my_hcp_slant.ref_green_h = hsv[0];
       my_hcp_slant.ref_green_s = hsv[1];

       if(calibrate_mode==1){
            AUO3D_imagePath_LR(PC_image_path(R),PC_image_path(G));
       }else if(calibrate_mode==0){
           Second_Monitor_mode(R_G);
       }

       hcp_slant_step->setMapping(calibrate_timer, 3);
       //stream<<"Color Contrast"<<"\n\n";
      //hcp_slant_step->setMapping(calibrate_timer, 7);  //test color contrast
       calibrate_timer->setInterval(800);
       calibrate_timer->setSingleShot(true);
       calibrate_timer->start();
   }
   //color contrast
   else if(step==3){
        int res;
        double color_constrat = 0.0;
        Mat tmp_frame;
        tmp_frame = single.current_frame.clone();
        res = Calibrate.set_image_by_camera(tmp_frame);
        color_constrat = Calibrate.get_Max_Color_Contrast();
        int int_color = color_constrat*100000;
        ui->hcp->setText(QString::fromStdString(to_string( my_hcp_slant.hcp)));

        if(color_constrat>my_hcp_slant.best_color_contrast){
            my_hcp_slant.best_color_contrast = color_constrat;
            my_hcp_slant.best_color_hcp = my_hcp_slant.hcp;
        }
        /*
        if (int_color <  COLOR_DISTANCE_THRESHOLD*100000) { //if value is not OK adjust the pitch
                    my_hcp_slant.hcp = my_hcp_slant.hcp + my_hcp_slant.color_distance_adjust_step;
                    if(calibrate_mode==1){
                        res = AUO3D_SendData(PanelData::HCP, &my_hcp_slant.hcp);
                    }else if(calibrate_mode==0){
                        res = AUO3D_FPGA_SendData(PanelData::HCP, &my_hcp_slant.hcp);
                        //res = AUO3D_FPGA_SendData(PanelData::WS_OVD, &my_hcp_slant.ws_ovd);
                    }
                    //res = AUO3D_SendData(PanelData::WS_OVD, &my_hcp_slant.ws_ovd);
                    ui->hcp->setText(QString::fromStdString(to_string( my_hcp_slant.hcp)));
                    ui->color_contrast->setText(QString::fromStdString(to_string(color_constrat)));
                    hcp_slant_step->setMapping(calibrate_timer, 3);
                    calibrate_timer->start();
         }
        */
        if (my_hcp_slant.color_search_time<20) { //if value is not OK adjust the pitch
            my_hcp_slant.color_search_time++;
            my_hcp_slant.hcp = my_hcp_slant.hcp + my_hcp_slant.color_distance_adjust_step;
            if(calibrate_mode==1){
                res = AUO3D_SendData(PanelData::HCP, &my_hcp_slant.hcp);
            }else if(calibrate_mode==0){
                res = AUO3D_FPGA_SendData(PanelData::HCP, &my_hcp_slant.hcp);
                //res = AUO3D_FPGA_SendData(PanelData::WS_OVD, &my_hcp_slant.ws_ovd);
            }
            //res = AUO3D_SendData(PanelData::WS_OVD, &my_hcp_slant.ws_ovd);
            ui->hcp->setText(QString::fromStdString(to_string( my_hcp_slant.hcp)));
            ui->color_contrast->setText(QString::fromStdString(to_string(color_constrat)));
            hcp_slant_step->setMapping(calibrate_timer, 3);
            calibrate_timer->start();
        }
        else{
            my_hcp_slant.color_search_time=0;
            my_hcp_slant.hcp = my_hcp_slant.best_color_hcp;
            ui->hcp->setText(QString::fromStdString(to_string( my_hcp_slant.hcp)));
            ui->color_contrast->setText(QString::fromStdString(to_string(my_hcp_slant.best_color_contrast)));
            if(calibrate_mode==1){
            res = AUO3D_SendData(PanelData::HCP, &my_hcp_slant.hcp);
            }else if(calibrate_mode==0){
                res = AUO3D_FPGA_SendData(PanelData::HCP, &my_hcp_slant.hcp);
                //res = AUO3D_FPGA_SendData(PanelData::WS_OVD, &my_hcp_slant.ws_ovd);
            }
            hcp_slant_step->setMapping(calibrate_timer, 4);
            calibrate_timer->start();
            /*
            if (my_hcp_slant.check_times < 1) { //check 3 times if the value is OK
                            my_hcp_slant.check_times++;
                            hcp_slant_step->setMapping(calibrate_timer, 3);
                            calibrate_timer->start();
            }
            else { //if pass 3 times start calibration


                ui->color_contrast->setText(QString::fromStdString(to_string(color_constrat)));
                my_hcp_slant.check_times = 0;
                hcp_slant_step->setMapping(calibrate_timer, 4);
                calibrate_timer->start();


                //my_hcp_slant.hcp = my_hcp_slant.hcp - my_hcp_slant.color_distance_adjust_step;
                //if(calibrate_mode==1){
                //    res = AUO3D_SendData(PanelData::HCP, &my_hcp_slant.hcp);
                //}else if(calibrate_mode==0){
                //    res = AUO3D_FPGA_SendData(PanelData::HCP, &my_hcp_slant.hcp);
                //    res = AUO3D_FPGA_SendData(PanelData::WS_OVD, &my_hcp_slant.ws_ovd);
                //}

            }
          */

        }

    }
   //slant
   else if(step==4){
        Mat tmp_frame;
        int res=0;
        double rg_ratio;
        int int_rg_ratio;

            //if slant is in range get rg
            tmp_frame = single.current_frame.clone();
            res = Calibrate.set_image_by_camera(tmp_frame);
            rg_ratio = Calibrate.get_RG_Ratio();
            int_rg_ratio = rg_ratio*100000;
            ui->rg_ratio->setText( QString::fromStdString(to_string(rg_ratio)));

        //if rg is better
       if (rg_ratio < my_hcp_slant.last_rg_ratio) { //if  rg ratio better than last one->  adjust next value
           my_hcp_slant.is_trend = true;
           my_hcp_slant.last_rg_ratio = rg_ratio;
           if(rg_ratio<my_hcp_slant.best_rg){
                my_hcp_slant.best_rg = rg_ratio;
                my_hcp_slant.best_hcp = my_hcp_slant.hcp;
                my_hcp_slant.best_slant = my_hcp_slant.slant;
           }
           ui->slant->setText(QString::fromStdString(to_string(my_hcp_slant.slant)));
           my_hcp_slant.slant = my_hcp_slant.slant + (my_hcp_slant.slant_adjust_step*my_hcp_slant.trend);
           if(calibrate_mode==1){
                res = AUO3D_SendData(PanelData::SLANT, &my_hcp_slant.slant);
           }else if(calibrate_mode==0){
               res = AUO3D_FPGA_SendData(PanelData::SLANT, &my_hcp_slant.slant);
           }

           hcp_slant_step->setMapping(calibrate_timer, 4);
           calibrate_timer->start();
       }
       //if not rg better
           else {
           //check twice
               if (my_hcp_slant.check_times < 1 ) {
               //if rg ratio get worse with same value at certain times
                   my_hcp_slant.check_times++;
                   hcp_slant_step->setMapping(calibrate_timer, 4);
                   calibrate_timer->start();
               }
               else {
                   //record edge
                   if (my_hcp_slant.trend == -1)
                       my_hcp_slant.min_slant = my_hcp_slant.slant;
                   else if( my_hcp_slant.trend ==1)
                       my_hcp_slant.max_slant = my_hcp_slant.slant;

                   //set back to last adjust value
                   my_hcp_slant.check_times = 0;
                   my_hcp_slant.slant = my_hcp_slant.slant - (my_hcp_slant.slant_adjust_step*my_hcp_slant.trend);
                   if(calibrate_mode==1){
                        res = AUO3D_SendData(PanelData::SLANT, &my_hcp_slant.slant);
                   }else if(calibrate_mode==0){
                       res = AUO3D_FPGA_SendData(PanelData::SLANT, &my_hcp_slant.slant);
                   }
                   ui->slant->setText(QString::fromStdString(to_string(my_hcp_slant.slant)));

                   if (!my_hcp_slant.is_trend && my_hcp_slant.trend ==1) {
                       //change search direction to  negative direction
                       my_hcp_slant.trend = -1;
                        my_hcp_slant.is_trend = false;
                       my_hcp_slant.slant = my_hcp_slant.slant + (my_hcp_slant.slant_adjust_step * my_hcp_slant.trend);
                       if(calibrate_mode==1){
                              res = AUO3D_SendData(PanelData::SLANT, &my_hcp_slant.slant);
                       }else if(calibrate_mode==0){
                             res = AUO3D_FPGA_SendData(PanelData::SLANT, &my_hcp_slant.slant);
                       }

                       ui->slant->setText(QString::fromStdString(to_string(my_hcp_slant.slant)));
                       hcp_slant_step->setMapping(calibrate_timer, 4);
                       calibrate_timer->start();
                   }
                   else{
                       //if check both direction
                       //set back trend
                       my_hcp_slant.is_trend = false;
                       my_hcp_slant.trend = 1;
                       //minor the adjust step
                       my_hcp_slant.slant_adjust_step = my_hcp_slant.slant_adjust_step - my_hcp_slant.slant_adjust_range; //ex:0.05-0.01

                       if (my_hcp_slant.slant_adjust_step >= my_hcp_slant.slant_adjust_range) {

                           //adjust with minor step
                           my_hcp_slant.slant = my_hcp_slant.slant + (my_hcp_slant.slant_adjust_step*my_hcp_slant.trend);
                           if(calibrate_mode==1){
                               res = AUO3D_SendData(PanelData::SLANT, & my_hcp_slant.slant);
                           }else if(calibrate_mode==0){
                                res = AUO3D_FPGA_SendData(PanelData::SLANT, & my_hcp_slant.slant);
                           }

                           ui->slant->setText(QString::fromStdString(to_string( my_hcp_slant.slant)));
                           //todo keep search in slant
                           hcp_slant_step->setMapping(calibrate_timer, 4);
                           calibrate_timer->start();
                       }
                       else {
                           //if adjust step too small
                            my_hcp_slant.slant_adjust_step =  my_hcp_slant.slant_adjust_step +  my_hcp_slant.slant_adjust_range;
                           //go to calibrate hcp
                           if ( my_hcp_slant.slant_adjust_range == my_hcp_slant.slant_adjust_value*0.01) {

                                my_hcp_slant.hcp_adjust_range =  my_hcp_slant.hcp_adjust_value * 0.01;
                                my_hcp_slant.hcp_adjust_step =  my_hcp_slant.hcp_adjust_range  *5;

                                my_hcp_slant.hcp =  my_hcp_slant.hcp + ( my_hcp_slant.hcp_adjust_step *  my_hcp_slant.trend);
                                if(calibrate_mode==1){
                                     res = AUO3D_SendData(PanelData::HCP, & my_hcp_slant.hcp);
                                }else if(calibrate_mode==0){
                                     res = AUO3D_FPGA_SendData(PanelData::HCP, & my_hcp_slant.hcp);
                                     //res = AUO3D_FPGA_SendData(PanelData::WS_OVD, &my_hcp_slant.ws_ovd);
                                     //res = AUO3D_FPGA_SendData(PanelData::WS_OVD, & my_hcp_slant.ws_ovd);
                                }

                               //todo finish slant
                               hcp_slant_step->setMapping(calibrate_timer, 5);
                               calibrate_timer->start();
                           }
                           //slant calibrate end
                           else {
                               my_hcp_slant.hcp = my_hcp_slant.hcp + (my_hcp_slant.hcp_adjust_step * my_hcp_slant.trend);
                               if(calibrate_mode==1){
                                    res = AUO3D_SendData(PanelData::HCP, & my_hcp_slant.hcp);
                               }else if(calibrate_mode==0){
                                   res = AUO3D_FPGA_SendData(PanelData::HCP, &my_hcp_slant.hcp);
                                   //res = AUO3D_FPGA_SendData(PanelData::WS_OVD, &my_hcp_slant.ws_ovd);
                                   //res = AUO3D_FPGA_SendData(PanelData::WS_OVD, &my_hcp_slant.ws_ovd);
                               }

                               //todo go to  hcp
                               hcp_slant_step->setMapping(calibrate_timer, 5);
                               calibrate_timer->start();
                           }
                       }
                   }
               }
           }



   }
   //hcp
   else if(step==5){
       Mat tmp_frame;
       int res;
       double rg_ratio;
       int int_rg_ratio;
       //get rg_ratio

           tmp_frame = single.current_frame.clone();
           res = Calibrate.set_image_by_camera(tmp_frame);
           rg_ratio = Calibrate.get_RG_Ratio();
           int_rg_ratio = rg_ratio*100000;
           ui->rg_ratio->setText( QString::fromStdString(to_string(rg_ratio)));


       //if rg_ratio get better
       if ( rg_ratio < my_hcp_slant.last_rg_ratio) {
           my_hcp_slant.is_trend = true;
           my_hcp_slant.last_rg_ratio = rg_ratio;
           if(rg_ratio<my_hcp_slant.best_rg){
                   my_hcp_slant.best_rg = rg_ratio;
                   my_hcp_slant.best_hcp = my_hcp_slant.hcp;
                   my_hcp_slant.best_slant = my_hcp_slant.slant;
           }
           //continue adjust hcp
           ui->hcp->setText(QString::fromStdString(to_string(my_hcp_slant.hcp)));
           my_hcp_slant.hcp = my_hcp_slant.hcp + (my_hcp_slant.hcp_adjust_step*my_hcp_slant.trend);
           if(calibrate_mode==0){
               res = AUO3D_FPGA_SendData(PanelData::HCP, &my_hcp_slant.hcp);
               //res = AUO3D_FPGA_SendData(PanelData::WS_OVD, &my_hcp_slant.ws_ovd);
               //res = AUO3D_FPGA_SendData(PanelData::WS_OVD, &ws_ovd);
           }
           else if(calibrate_mode==1){
                res = AUO3D_SendData(PanelData::HCP, &my_hcp_slant.hcp);
           }

          hcp_slant_step->setMapping(calibrate_timer, 5);
          calibrate_timer->start();
       }
       else {
           //if rg ratio get worse with same value at certain times
           if ( my_hcp_slant.check_times < 1) {
               my_hcp_slant.check_times++;
               hcp_slant_step->setMapping(calibrate_timer, 5);
               calibrate_timer->start();
           }
           else {

               if ( my_hcp_slant.trend == 1)
                   my_hcp_slant.max_hcp = my_hcp_slant.hcp;
               else if ( my_hcp_slant.trend == -1)
                   my_hcp_slant.min_hcp = my_hcp_slant.hcp;
               //set back to last adjust value
               my_hcp_slant.check_times = 0;
               my_hcp_slant.hcp= my_hcp_slant.hcp - (my_hcp_slant.hcp_adjust_step*my_hcp_slant.trend);
               if(calibrate_mode==0){
                   res = AUO3D_FPGA_SendData(PanelData::HCP, &my_hcp_slant.hcp);
                   //res = AUO3D_FPGA_SendData(PanelData::WS_OVD, &my_hcp_slant.ws_ovd);
               }
               else if(calibrate_mode==1){
                    res = AUO3D_SendData(PanelData::HCP, &my_hcp_slant.hcp);

               }

               ui->hcp->setText(QString::fromStdString(to_string(my_hcp_slant.hcp)));
               //Log("HCP calibration success", 'S');

               if (!my_hcp_slant.is_trend && my_hcp_slant.trend == 1) {
                   //change search direction to  negative direction
                   my_hcp_slant.trend = -1;
                   my_hcp_slant.is_trend =false;
                   my_hcp_slant.hcp = my_hcp_slant.hcp + (my_hcp_slant.hcp_adjust_step * my_hcp_slant.trend);
                   if(calibrate_mode==0){
                       res = AUO3D_FPGA_SendData(PanelData::HCP, &my_hcp_slant.hcp);
                       //res = AUO3D_FPGA_SendData(PanelData::WS_OVD, &my_hcp_slant.ws_ovd);
                       //res = AUO3D_FPGA_SendData(PanelData::WS_OVD, &ws_ovd);
                   }
                   else if(calibrate_mode==1){
                        res = AUO3D_SendData(PanelData::HCP, &my_hcp_slant.hcp);

                   }
                   ui->hcp->setText(QString::fromStdString(to_string(my_hcp_slant.hcp)));
                   hcp_slant_step->setMapping(calibrate_timer, 5);
                   calibrate_timer->start();
               }
               else {
                   my_hcp_slant.is_trend = 0;
                   my_hcp_slant.trend = 1;
                   //minor the adjust step
                   my_hcp_slant.hcp_adjust_step = my_hcp_slant.hcp_adjust_step - my_hcp_slant.hcp_adjust_range;

                   if (my_hcp_slant.hcp_adjust_step >= my_hcp_slant.hcp_adjust_range) {
                       my_hcp_slant.hcp = my_hcp_slant.hcp + (my_hcp_slant.hcp_adjust_step * my_hcp_slant.trend);
                       if(calibrate_mode==0){
                           res = AUO3D_FPGA_SendData(PanelData::HCP, &my_hcp_slant.hcp);
                           //res = AUO3D_FPGA_SendData(PanelData::WS_OVD, &my_hcp_slant.ws_ovd);
                       }
                       else if(calibrate_mode==1){
                            res = AUO3D_SendData(PanelData::HCP, &my_hcp_slant.hcp);
                            //res = AUO3D_FPGA_SendData(PanelData::WS_OVD, &my_hcp_slant.ws_ovd);
                       }
                       ui->hcp->setText(QString::fromStdString(to_string(my_hcp_slant.hcp)));
                       hcp_slant_step->setMapping(calibrate_timer, 5);
                       calibrate_timer->start();
                   }
                   else {
                       if ( my_hcp_slant.hcp_adjust_range == my_hcp_slant.hcp_adjust_value*0.1) {
                           my_hcp_slant.slant_adjust_range = my_hcp_slant.slant_adjust_value * 0.01;
                           my_hcp_slant.slant_adjust_step = my_hcp_slant.slant_adjust_range *5;
                           my_hcp_slant.slant = my_hcp_slant.slant + (my_hcp_slant.slant_adjust_step * my_hcp_slant.trend);
                           if(calibrate_mode==0){
                               res = AUO3D_FPGA_SendData(PanelData::SLANT, &my_hcp_slant.slant);

                           }
                           else if(calibrate_mode==1){
                               res = AUO3D_SendData(PanelData::SLANT, &my_hcp_slant.slant);
                           }

                           ui->slant->setText(QString::fromStdString(to_string(my_hcp_slant.slant)));
                           hcp_slant_step->setMapping( calibrate_timer, 4);
                            calibrate_timer->start();
                       }
                       else {                           
                           //hcp/slant calibrate finish
                           ui->start_calibrate_hcp->setText(QString::fromLocal8Bit("校正完成"));
                           ui->start_calibrate_hcp->setEnabled(true); //校正按鈕
                           ui->Calibration_Function_Box->setEnabled(true); //校正選單

                           my_hcp_slant.hcp = my_hcp_slant.best_hcp;
                           my_hcp_slant.slant = my_hcp_slant.best_slant;
                           if(calibrate_mode==0){
                               res = AUO3D_FPGA_SendData(PanelData::HCP, &my_hcp_slant.hcp);
                                res = AUO3D_FPGA_SendData(PanelData::SLANT, &my_hcp_slant.slant);

                           }
                           else if(calibrate_mode==1){
                               res = AUO3D_SendData(PanelData::HCP, &my_hcp_slant.hcp);
                               res = AUO3D_SendData(PanelData::SLANT, &my_hcp_slant.slant);

                           }
                           ui->rg_ratio->setText( QString::fromStdString(to_string(my_hcp_slant.best_rg)));



                           ui->calibrate_next->show(); //下一步
                           ui->hcp->setText(QString::fromStdString(to_string(my_hcp_slant.hcp)));
                           ui->slant->setText(QString::fromStdString(to_string(my_hcp_slant.slant)));                         
                           ui->hcp_slant->setStyleSheet("QPushButton{background-color: rgb(46, 125, 161);}");
                           ui->hcp_slant->setEnabled(true);

                           my_hcp_slant.best_color_contrast=0.0;
                           my_hcp_slant.best_color_hcp=0.0;

                           my_hcp_slant.is_calibrating = false; //正在校正
                           my_calibrate.slant = my_hcp_slant.slant;
                           my_calibrate.hcp = my_hcp_slant.hcp;
                           disconnect(calibrate_timer, SIGNAL(timeout()),  hcp_slant_step, SLOT(map())); //Timer 連接

                           double ws_ovd = my_calibrate.ws_ovd;
                           double xoff_lens = my_calibrate.xoff_lens;
                           double a = my_calibrate.a;
                           double b = my_calibrate.b;
                           double c = my_calibrate.c;
                           double e1 = my_calibrate.e1;
                           double e2 = my_calibrate.e2;
                           my_hcp_slant.hcp_adjust_range = my_hcp_slant.hcp_adjust_value * 0.1;
                           my_hcp_slant.hcp_adjust_step= my_hcp_slant.hcp_adjust_value / 2;
                           my_hcp_slant.slant_adjust_step=my_hcp_slant.slant_adjust_value / 2;
                           my_hcp_slant.slant_adjust_range=my_hcp_slant.slant_adjust_value * 0.1;
                           my_hcp_slant.best_slant=0.0;
                           my_hcp_slant.best_hcp=0.0;
                           my_hcp_slant.best_rg=999;



                   
                           if(calibrate_mode==1){
                               AUO3D_SendData(PanelData::HCP, &my_hcp_slant.hcp);
                               AUO3D_SendData(PanelData::SLANT, &my_hcp_slant.slant);
                               AUO3D_SendData(PanelData::WS_OVD, &ws_ovd);
                               AUO3D_SendData(PanelData::XOFF_LENS, &xoff_lens);
                               AUO3D_SendData(PanelData::A, &a);
                               AUO3D_SendData(PanelData::B, &b);
                               AUO3D_SendData(PanelData::C, &c);
                               AUO3D_SendData(PanelData::E1, &e1);
                               AUO3D_SendData(PanelData::E2, &e2);                            
                       
                           }else if(calibrate_mode==0){
                               AUO3D_FPGA_SendData(PanelData::HCP, &my_hcp_slant.hcp);
                               AUO3D_FPGA_SendData(PanelData::SLANT, &my_hcp_slant.slant);
                               AUO3D_FPGA_SendData(PanelData::WS_OVD, &ws_ovd);
                               AUO3D_FPGA_SendData(PanelData::XOFF_LENS, &xoff_lens);
                               AUO3D_FPGA_SendData(PanelData::A, &a);
                               AUO3D_FPGA_SendData(PanelData::B, &b);
                               AUO3D_FPGA_SendData(PanelData::C, &c);
                               AUO3D_FPGA_SendData(PanelData::E1, &e1);
                               AUO3D_FPGA_SendData(PanelData::E2, &e2);
                           }


                       }
                   }

               }
           }
       }



   }

   //color contrast test
   else if(step==6){
       int res;
       double color_constrat = 0.0;
       Mat tmp_frame;
       tmp_frame = single.current_frame.clone();
       res = Calibrate.set_image_by_camera(tmp_frame);
       color_constrat = Calibrate.get_Max_Color_Contrast();

       if(count_slant<21){

           if(count_hcp<21){
               stream<<QString::number(color_constrat)<<",";
               my_hcp_slant.hcp = my_hcp_slant.hcp + 0.001;
               if(calibrate_mode==1){
                   res = AUO3D_SendData(PanelData::HCP, &my_hcp_slant.hcp);
               }else if(calibrate_mode==0){
                   res = AUO3D_FPGA_SendData(PanelData::HCP, &my_hcp_slant.hcp);
               }
               count_hcp++;

               hcp_slant_step->setMapping(calibrate_timer, 6);
               calibrate_timer->start();

           }else{
               count_hcp=0;
               count_slant++;
               stream<<"\n";
               my_hcp_slant.hcp = my_hcp_slant.hcp-0.021;
               if(calibrate_mode==1){
                   res = AUO3D_SendData(PanelData::HCP, &my_hcp_slant.hcp);
               }else if(calibrate_mode==0){
                   res = AUO3D_FPGA_SendData(PanelData::HCP, &my_hcp_slant.hcp);
               }
               my_hcp_slant.slant =  my_hcp_slant.slant+0.01;
               if(calibrate_mode==1){
                   res = AUO3D_SendData(PanelData::SLANT, &my_hcp_slant.slant);
               }else if(calibrate_mode==0){
                   res = AUO3D_FPGA_SendData(PanelData::SLANT, &my_hcp_slant.slant);
               }
               hcp_slant_step->setMapping(calibrate_timer, 6);
               calibrate_timer->start();
           }
       }else{
           count_slant=0;
           my_hcp_slant.slant = my_hcp_slant.slant-0.21;
           if(calibrate_mode==1){
               res = AUO3D_SendData(PanelData::SLANT, &my_hcp_slant.slant);
           }else if(calibrate_mode==0){
               res = AUO3D_FPGA_SendData(PanelData::SLANT, &my_hcp_slant.slant);
           }
           //file.close();
           stream<<"rg RATIO "<<"\n";
           hcp_slant_step->setMapping(calibrate_timer, 7);
           calibrate_timer->start();

       }

       ui->hcp->setText(QString::fromStdString(to_string(my_hcp_slant.hcp)));
       ui->slant->setText(QString::fromStdString(to_string(my_hcp_slant.slant)));



   }
   //rg ratio test
   else if(step==7){
       Mat tmp_frame;
       int res=0;
       double rg_ratio;
       int int_rg_ratio;

       //if slant is in range get rg
       tmp_frame = single.current_frame.clone();
       res = Calibrate.set_image_by_camera(tmp_frame);
       rg_ratio = Calibrate.get_RG_Ratio();


       if(count_slant<21){

           if(count_hcp<21){
               stream<<QString::number( rg_ratio)<<",";
               my_hcp_slant.hcp = my_hcp_slant.hcp + 0.0001;
               if(calibrate_mode==1){
                   res = AUO3D_SendData(PanelData::HCP, &my_hcp_slant.hcp);
               }else if(calibrate_mode==0){
                   res = AUO3D_FPGA_SendData(PanelData::HCP, &my_hcp_slant.hcp);
               }
               count_hcp++;

               hcp_slant_step->setMapping(calibrate_timer, 7);
               calibrate_timer->start();

           }else{
               count_hcp=0;
               count_slant++;
               stream<<"\n";
               my_hcp_slant.hcp = my_hcp_slant.hcp-0.0021;
               if(calibrate_mode==1){
                   res = AUO3D_SendData(PanelData::HCP, &my_hcp_slant.hcp);
               }else if(calibrate_mode==0){
                   res = AUO3D_FPGA_SendData(PanelData::HCP, &my_hcp_slant.hcp);
               }
               my_hcp_slant.slant =  my_hcp_slant.slant+0.001;
               if(calibrate_mode==1){
                   res = AUO3D_SendData(PanelData::SLANT, &my_hcp_slant.slant);
               }else if(calibrate_mode==0){
                   res = AUO3D_FPGA_SendData(PanelData::SLANT, &my_hcp_slant.slant);
               }
               hcp_slant_step->setMapping(calibrate_timer, 7);
               calibrate_timer->start();
           }
       }else{
           count_slant=0;
           my_hcp_slant.slant = my_hcp_slant.slant-0.021;
           if(calibrate_mode==1){
               res = AUO3D_SendData(PanelData::SLANT, &my_hcp_slant.slant);
           }else if(calibrate_mode==0){
               res = AUO3D_FPGA_SendData(PanelData::SLANT, &my_hcp_slant.slant);
           }
           file.close();
            ui->start_calibrate_hcp->setText(QString::fromLocal8Bit("校正完成"));

       }


       ui->hcp->setText(QString::fromStdString(to_string(my_hcp_slant.hcp)));
       ui->slant->setText(QString::fromStdString(to_string(my_hcp_slant.slant)));

   }
}

//---------------------------------------------------Page 4 Function (Xoff Lens)
void MainWindow::start_xoff_lens(){
    connect(calibrate_timer, SIGNAL(timeout()),  xoff_lens_step, SLOT(map()));
    xoff_lens_step->setMapping(calibrate_timer, 0);
    
    double hcp = my_calibrate.hcp;
    double slant = my_calibrate.slant;
    double ws_ovd = 0;
    my_xoff_lens.Xoff_len = parameters.x_off_lens;
    double a = 0;
    double b = 0;
    double c = 0;
    double e1 = 0;
    double e2 = 0;

    if(calibrate_mode==1){
        AUO3D_SendData(PanelData::HCP, &hcp);
        AUO3D_SendData(PanelData::SLANT, &slant);
        AUO3D_SendData(PanelData::WS_OVD, &ws_ovd);
        AUO3D_SendData(PanelData::XOFF_LENS, &my_xoff_lens.Xoff_len);
        AUO3D_SendData(PanelData::A, &a);
        AUO3D_SendData(PanelData::B, &b);
        AUO3D_SendData(PanelData::C, &c);
        AUO3D_SendData(PanelData::E1, &e1);
        AUO3D_SendData(PanelData::E2, &e2);
    }else if(calibrate_mode==0){
        AUO3D_FPGA_SendData(PanelData::HCP, &my_hcp_slant.hcp);
        AUO3D_FPGA_SendData(PanelData::SLANT, &my_hcp_slant.slant);
        AUO3D_FPGA_SendData(PanelData::WS_OVD, &ws_ovd);
        AUO3D_FPGA_SendData(PanelData::XOFF_LENS, &my_xoff_lens.Xoff_len);
        AUO3D_FPGA_SendData(PanelData::A, &a);
        AUO3D_FPGA_SendData(PanelData::B, &b);
        AUO3D_FPGA_SendData(PanelData::C, &c);
        AUO3D_FPGA_SendData(PanelData::E1, &e1);
        AUO3D_FPGA_SendData(PanelData::E2, &e2);
    }

    my_xoff_lens.Xoff_adjust_step = my_xoff_lens.Xoff_adjust_value;

    calibrate_timer->start();
    my_xoff_lens.is_calibrating = true;
    ui->start_calibrate_xoff_lens->setText(QString::fromLocal8Bit("校正中...."));
    ui->start_calibrate_xoff_lens->setEnabled(false);
    ui->Calibration_Function_Box->setEnabled(false);
}

void MainWindow::calibrate_xoff_lens(int step){
     int res = 0;
    if(step==0){
        my_xoff_lens.adjust_finish=false;
        my_xoff_lens.Xoff_len = parameters.x_off_lens;
        if(calibrate_mode==0){
            res = AUO3D_FPGA_SendData(PanelData::XOFF_LENS, &my_xoff_lens.Xoff_len);
            Second_Monitor_mode(W_B);

        }else if(calibrate_mode==1){
            res = AUO3D_SendData(PanelData::XOFF_LENS, &my_xoff_lens.Xoff_len);
             AUO3D_imagePath_LR(PC_image_path(W),PC_image_path(B));

        }
        ui->xoff_lens->setText(QString::number(my_xoff_lens.Xoff_len));
        xoff_lens_step->setMapping( calibrate_timer, 1);
        calibrate_timer->start();

    }else if(step==1){

        Calibrate.image_1 = single.current_frame.clone();
        if(calibrate_mode==0){
            Second_Monitor_mode(B_W);

        }else if(calibrate_mode==1){
             AUO3D_imagePath_LR(PC_image_path(B),PC_image_path(W));

        }
        xoff_lens_step->setMapping( calibrate_timer, 2);
        calibrate_timer->start();

    }
    else if(step==2){

         Calibrate.image_2 = single.current_frame.clone();
         Calibrate.x_off_filter = Calibrate.get_xoff_filter(Calibrate.image_1,Calibrate.image_2);
         Calibrate.normalize_filter(&Calibrate.normalize_data,Calibrate.x_off_filter);
         int pix_dis = Calibrate.x_off_filter.cols;
         for(int i=0;i<Calibrate.x_off_filter.cols;i++){
             if(Calibrate.normalize_data.binary_image.at<float>(Calibrate.x_off_filter.rows/2-1,i,0)
                     +Calibrate.normalize_data.binary_image.at<float>(Calibrate.x_off_filter.rows/2-1,i+1,0)==1
                     &&Calibrate.normalize_data.binary_image.at<float>(Calibrate.x_off_filter.rows/2-1,i,0)==0
                     ){

                 if (abs(i - 960) <= pix_dis) {
                        pix_dis = abs(i - 960);
                        Calibrate.normalize_data.center.x = i;
                    }

             }

         }
         ui->pix_dis->setText(QString::number(pix_dis));

         if(!my_xoff_lens.adjust_finish){
                 if(Calibrate.normalize_data.center.x-960<0){
                     my_xoff_lens.adjust_direction =1;
                 }else{
                      my_xoff_lens.adjust_direction =-1;
                 }

                 if(my_xoff_lens.first_pixel_distance==0){
                     my_xoff_lens.first_pixel_distance = pix_dis;
                     my_xoff_lens.Xoff_len = my_xoff_lens.Xoff_len + (my_xoff_lens.Xoff_adjust_step*my_xoff_lens.adjust_direction);

                     if(calibrate_mode==0){
                         res = AUO3D_FPGA_SendData(PanelData::XOFF_LENS, &my_xoff_lens.Xoff_len);
                         Second_Monitor_mode(W_B);

                     }else if(calibrate_mode==1){
                         res = AUO3D_SendData(PanelData::XOFF_LENS, &my_xoff_lens.Xoff_len);
                         AUO3D_imagePath_LR(PC_image_path(W),PC_image_path(B));
                     }
                     ui->xoff_lens->setText(QString::number(my_xoff_lens.Xoff_len));
                     xoff_lens_step->setMapping( calibrate_timer,1);
                     calibrate_timer->start();

                 }else{
                     my_xoff_lens.adjust_finish = true;
                     my_xoff_lens.Xoff_adjust_step = (pix_dis / (my_xoff_lens.first_pixel_distance - pix_dis) )* my_xoff_lens.Xoff_adjust_step;
                     my_xoff_lens.Xoff_len = my_xoff_lens.Xoff_len+ (my_xoff_lens.Xoff_adjust_step * my_xoff_lens.adjust_direction);
                     if(calibrate_mode==0){
                         res = AUO3D_FPGA_SendData(PanelData::XOFF_LENS, &my_xoff_lens.Xoff_len);
                         Second_Monitor_mode(W_B);

                     }else if(calibrate_mode==1){
                         res = AUO3D_SendData(PanelData::XOFF_LENS, &my_xoff_lens.Xoff_len);
                         AUO3D_imagePath_LR(PC_image_path(W),PC_image_path(B));
                     }
                     ui->xoff_lens->setText(QString::number(my_xoff_lens.Xoff_len));
                     xoff_lens_step->setMapping( calibrate_timer,1);
                     calibrate_timer->start();
                }

         }else{
             disconnect(calibrate_timer, SIGNAL(timeout()),  xoff_lens_step, SLOT(map()));
             my_xoff_lens.is_calibrating = false;
             my_calibrate.xoff_lens = my_xoff_lens.Xoff_len;
             ui->start_calibrate_xoff_lens->setText(QString::fromLocal8Bit("校正完成，點擊重新校正"));
             ui->Xoff_len->setStyleSheet("QPushButton{background-color: rgb(46, 125, 161);}");
             ui->Xoff_len->setEnabled(true);
             ui->start_calibrate_xoff_lens->setEnabled(true);
             ui->calibrate_next->show();
             ui->Calibration_Function_Box->setEnabled(true);
             
             my_xoff_lens.first_pixel_distance = 0.0;
             my_xoff_lens.adjust_direction =0;
             
             
             double hcp = my_calibrate.hcp;
             double slant = my_calibrate.slant;
             double ws_ovd = my_calibrate.ws_ovd;
             double xoff_lens = my_calibrate.xoff_lens;
             double a = my_calibrate.a;
             double b = my_calibrate.b;
             double c = my_calibrate.c;
             double e1 = my_calibrate.e1;
             double e2 = my_calibrate.e2;
     
             if(calibrate_mode==1){
                 AUO3D_SendData(PanelData::HCP, &hcp);
                 AUO3D_SendData(PanelData::SLANT, &slant);
                 AUO3D_SendData(PanelData::WS_OVD, &ws_ovd);
                 AUO3D_SendData(PanelData::XOFF_LENS, &xoff_lens);
                 AUO3D_SendData(PanelData::A, &a);
                 AUO3D_SendData(PanelData::B, &b);
                 AUO3D_SendData(PanelData::C, &c);
                 AUO3D_SendData(PanelData::E1, &e1);
                 AUO3D_SendData(PanelData::E2, &e2);                                         
             }else if(calibrate_mode==0){
                 AUO3D_FPGA_SendData(PanelData::HCP, &my_hcp_slant.hcp);
                 AUO3D_FPGA_SendData(PanelData::SLANT, &my_hcp_slant.slant);
                 AUO3D_FPGA_SendData(PanelData::WS_OVD, &ws_ovd);
                 AUO3D_FPGA_SendData(PanelData::XOFF_LENS, &xoff_lens);
                 AUO3D_FPGA_SendData(PanelData::A, &a);
                 AUO3D_FPGA_SendData(PanelData::B, &b);
                 AUO3D_FPGA_SendData(PanelData::C, &c);
                 AUO3D_FPGA_SendData(PanelData::E1, &e1);
                 AUO3D_FPGA_SendData(PanelData::E2, &e2);
             }
             
             

         }





    }

}

//---------------------------------------------------Page 5 Function (WS OVD)
void MainWindow::start_ws_ovd(){
        connect(calibrate_timer, SIGNAL(timeout()),  ws_ovd_step, SLOT(map()));
        ws_ovd_step->setMapping(calibrate_timer, 0);
        calibrate_timer->start();
        my_ws_ovd.is_calibrating=true;
        ui->start_calibrate_ws_ovd->setText(QString::fromLocal8Bit("校正中...."));
        ui->start_calibrate_ws_ovd->setEnabled(false);
        ui->Calibration_Function_Box->setEnabled(false);
        /*
        double vd = parameters.vd_far;
        if(calibrate_mode==0){
             AUO3D_FPGA_SendData(PanelData::VD, &vd);


        }else if(calibrate_mode==1){
            AUO3D_SendData(PanelData::VD, &vd);

        }
        */

}

void MainWindow::calibrate_ws_ovd(int step){
    int res;
    if(step==0){
        my_ws_ovd.rg_ratio_check_times=0;
        my_ws_ovd.ws_ovd=parameters.WS_OVD;
        if(calibrate_mode==1){            
        res = AUO3D_SendData(PanelData::WS_OVD, &my_ws_ovd.ws_ovd);
        AUO3D_imagePath_LR(PC_image_path(R),PC_image_path(G));

        }else if(calibrate_mode==0){
            res = AUO3D_FPGA_SendData(PanelData::WS_OVD, &my_ws_ovd.ws_ovd);
            Second_Monitor_mode(R_G);
        }

        ui->ws_ovd->setText(QString::number(my_ws_ovd.ws_ovd));
        ws_ovd_step->setMapping( calibrate_timer, 1);
        calibrate_timer->start();

    }else if(step==1){
        double rg_ratio;
        my_ws_ovd.tmp_frame = single.current_frame.clone();
        res = Calibrate.set_image_by_camera(my_ws_ovd.tmp_frame);
        rg_ratio = Calibrate.get_RG_Ratio();
        ui->ws_rg_ratio->setText(QString::number(rg_ratio));
        if(rg_ratio<my_ws_ovd.best_rg_ratio){
            my_ws_ovd.best_rg_ratio = rg_ratio;
            my_ws_ovd.rg_ratio_check_times=0;
            my_ws_ovd.ws_ovd =  my_ws_ovd.ws_ovd+ my_ws_ovd.ws_ovd_adjust_step;
            if(calibrate_mode==1){
                res = AUO3D_SendData(PanelData::WS_OVD, &my_ws_ovd.ws_ovd);
            }else if(calibrate_mode==0){
                res = AUO3D_FPGA_SendData(PanelData::WS_OVD, &my_ws_ovd.ws_ovd);
            }
            ui->ws_ovd->setText(QString::number(my_ws_ovd.ws_ovd));
            ws_ovd_step->setMapping( calibrate_timer, 1);
            calibrate_timer->start();
        }
        else{
            if(my_ws_ovd.rg_ratio_check_times<3){
                my_ws_ovd.rg_ratio_check_times++;
                ws_ovd_step->setMapping( calibrate_timer, 1);
                calibrate_timer->start();
            }else{
                my_ws_ovd.ws_ovd =  my_ws_ovd.ws_ovd- my_ws_ovd.ws_ovd_adjust_step;
                if(calibrate_mode==1){
                    res = AUO3D_SendData(PanelData::WS_OVD, &my_ws_ovd.ws_ovd);
                }else if(calibrate_mode==0){
                    res = AUO3D_FPGA_SendData(PanelData::WS_OVD, &my_ws_ovd.ws_ovd);
                }
                my_calibrate.ws_ovd=my_ws_ovd.ws_ovd;
                my_ws_ovd.is_calibrating=false;
                disconnect(calibrate_timer, SIGNAL(timeout()), ws_ovd_step, SLOT(map()));
                ui->start_calibrate_ws_ovd->setText(QString::fromLocal8Bit("校正完成，點此可重新校正"));
                ui->to_ws_ovd->setStyleSheet("QPushButton{background-color: rgb(46, 125, 161);}");
                ui->to_ws_ovd->setEnabled(true);
                ui->start_calibrate_ws_ovd->setEnabled(true);
                ui->Calibration_Function_Box->setEnabled(true);
                ui->calibrate_next->show();

            }


        }


    }


}

void MainWindow::ws_test(){
    int res;
    double vd = 0;//ui->WS_TEST->text().toDouble();
    if(calibrate_mode==0){
        AUO3D_FPGA_SendData(PanelData::VD, &vd);


    }else if(calibrate_mode==1){
        AUO3D_SendData(PanelData::VD, &vd);

    }




}
//---------------------------------------------------Page 6 Function (VIEW ZONE)
void MainWindow::start_roi_detection(int camera_num){

    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(200, 200), cv::Point(-1, -1));
        Mat src;
        Mat gray;
        Mat mask;
        Mat contour;
        Rect tmp_rect;


            while (!dual_1.is_close && !dual_2.is_close) {
                    if(!dual_1.stop_update_roi && !dual_2.stop_update_roi){

            if (!dual_1.current_frame.empty() && !dual_2.current_frame.empty()) {

                 if(camera_num==1){
                    src = dual_1.current_frame.clone();
                    int area = 0;
                    cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
                    cv::GaussianBlur(gray, mask, cv::Size(3, 3), 0);
                    cv::threshold(mask, mask, 50, 255, cv::THRESH_BINARY); // ? binary image
                    cv::morphologyEx(mask, contour, cv::MORPH_OPEN, kernel);  //? erode image
                    cv::Canny(contour, contour, 120, 250);
                    cv::findContours(contour,dual_1.coutours, dual_1.hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

                    //make rect area
                    if (!dual_1.coutours.empty()) {
                        for (int i = 0; i < dual_1.coutours.size(); i++) {
                            tmp_rect = cv::boundingRect(dual_1.coutours[i]);
                            if (tmp_rect.height * tmp_rect.width > area) {
                                area = tmp_rect.height * tmp_rect.width;
                                dual_1.bounding_rect = tmp_rect;
                            }
                        }
                        dual_1.is_contour = true;
                    }

                }


                 else if(camera_num==2){

                     src = dual_2.current_frame.clone();
                     int area = 0;
                     cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
                     cv::GaussianBlur(gray, mask, cv::Size(3, 3), 0);
                     cv::threshold(mask, mask, 50, 255, cv::THRESH_BINARY); // ? binary image
                     cv::morphologyEx(mask, contour, cv::MORPH_OPEN, kernel);  //? erode image
                     cv::Canny(contour, contour, 120, 250);
                     cv::findContours(contour,dual_2.coutours, dual_2.hierarchy, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

                     //make rect area
                     if (!dual_2.coutours.empty()) {
                         for (int i = 0; i < dual_2.coutours.size(); i++) {
                             tmp_rect = cv::boundingRect(dual_2.coutours[i]);
                             if (tmp_rect.height * tmp_rect.width > area) {
                                 area = tmp_rect.height * tmp_rect.width;
                                 dual_2.bounding_rect = tmp_rect;
                             }
                         }
                         dual_2.is_contour = true;
                     }


                  }

            }

            }

            }

}

void MainWindow::start_view_zone(){
    if(dual_1.is_contour&&dual_2.is_contour){
         ui->view_zone_warn->setText(QString::fromLocal8Bit(""));
        dual_1.stop_update_roi=true;
        dual_2.stop_update_roi=true;
        connect(calibrate_timer, SIGNAL(timeout()),  view_zone_step, SLOT(map()));
        my_view_zone.is_calibrating=true;
        ui->start_calibrate_view_zone->setText(QString::fromLocal8Bit("校正中...."));
        ui->start_calibrate_view_zone->setEnabled(false);
        ui->Calibration_Function_Box->setEnabled(false);

        my_view_zone.tmp_xoff_len = my_calibrate.xoff_lens;
        /**/
          double WS=0;
        if(calibrate_mode==1){
             AUO3D_SendData(PanelData::WS_OVD, &WS);
        }else if(calibrate_mode==0){
            AUO3D_FPGA_SendData(PanelData::WS_OVD, &WS);
        }
        /*
        view_zone_step->setMapping(calibrate_timer, 0);
        calibrate_timer->setInterval(1200);
        calibrate_timer->start();
*/

        //gray scale test

        view_zone_step->setMapping(calibrate_timer, 6);
        calibrate_timer->setInterval(800);
        calibrate_timer->start();


    }else{
        ui->view_zone_warn->setText(QString::fromLocal8Bit("未偵測到ROI"));
    }

}

void MainWindow:: calibrate_view_zone(int step){


    if(step==0){
        //decide left and right
        Mat dual_1_roi = dual_1.current_frame(dual_1.bounding_rect).clone();
        Mat dual_2_roi = dual_2.current_frame(dual_2.bounding_rect).clone();

        int side_left = Calibrate.find_right_and_left_camera(dual_1_roi,dual_2_roi);

        if(side_left==1){

            dual_1.side = 1;
            dual_2.side = 2;
            ui->eye_pos_1_view->setText(QString::fromLocal8Bit("右"));
            ui->eye_pos_2_view->setText(QString::fromLocal8Bit("左"));
        }else {

            dual_2.side = 1;
            dual_1.side = 2;
            ui->eye_pos_1_view->setText(QString::fromLocal8Bit("左"));
            ui->eye_pos_2_view->setText(QString::fromLocal8Bit("右"));
        }

/*
        if(calibrate_mode==1){
            AUO3D_imagePath_LR(PC_image_path(R),PC_image_path(G));
        }else if(calibrate_mode==0){
            Second_Monitor_mode(R_G);
        }

*/
        //RG View Zone
        //view_zone_step->setMapping(calibrate_timer, 1);
        //calibrate_timer->start();


        //Cross talk View Zone
        my_view_zone.tmp_xoff_len = my_calibrate.view_zone;
        if(calibrate_mode==1){
            AUO3D_imagePath_LR(PC_image_path(W),PC_image_path(B));
        }else if(calibrate_mode==0){
            Second_Monitor_mode(W_B);
        }

        view_zone_step->setMapping(calibrate_timer,11);
        calibrate_timer->start();

    }

    // RG viewzone verfi
    else if(step == 1){

        //set ref
        if(dual_1.side==1){


            Mat ref_green = dual_1.current_frame.clone();
            Mat ref_red = dual_2.current_frame.clone();



            Calibrate.set_ref_by_camera(ref_green(dual_1.bounding_rect),'G');
            Calibrate.set_ref_by_camera(ref_red(dual_2.bounding_rect),'R');

            //show on UI
            int *ref_g = new int[3];
            int *ref_r = new int[3];        
            ref_g = Calibrate.get_ref_rgb('G');
            ref_r = Calibrate.get_ref_rgb('R');
            Mat display_ref_g(ui->ref_green_view_zone->height(), ui->ref_green_view_zone->width(), CV_8UC3, Scalar(ref_g[0], ref_g[1], ref_g[2]));
            QImage display_g = QImage((uchar*)(display_ref_g.data), display_ref_g.cols, display_ref_g.rows, display_ref_g.step, QImage::Format_RGB888);
            ui->ref_green_view_zone->setPixmap(QPixmap::fromImage(display_g));
            Mat display_ref_r(ui->ref_red_view_zone->height(), ui->ref_red_view_zone->width(), CV_8UC3, Scalar(ref_r[0], ref_r[1], ref_r[2]));
            QImage display_r = QImage((uchar*)(display_ref_r.data), display_ref_r.cols, display_ref_r.rows, display_ref_r.step, QImage::Format_RGB888);
            ui->ref_red_view_zone->setPixmap(QPixmap::fromImage(display_r));

        }else{



            Mat ref_red = dual_1.current_frame.clone();
            Mat ref_green = dual_2.current_frame.clone();

            Calibrate.set_ref_by_camera(ref_red(dual_1.bounding_rect),'R');
            Calibrate.set_ref_by_camera(ref_green(dual_2.bounding_rect),'G');

             //show on UI
            int *ref_g = new int[3];
            int *ref_r = new int[3];
            ref_g = Calibrate.get_ref_rgb('G');
            ref_r = Calibrate.get_ref_rgb('R');
            Mat display_ref_g(ui->ref_green_view_zone->height(), ui->ref_green_view_zone->width(), CV_8UC3, Scalar(ref_g[0], ref_g[1], ref_g[2]));
            QImage display_g = QImage((uchar*)(display_ref_g.data), display_ref_g.cols, display_ref_g.rows, display_ref_g.step, QImage::Format_RGB888);
            ui->ref_green_view_zone->setPixmap(QPixmap::fromImage(display_g));
            Mat display_ref_r(ui->ref_red_view_zone->height(), ui->ref_red_view_zone->width(), CV_8UC3, Scalar(ref_r[0], ref_r[1], ref_r[2]));
            QImage display_r = QImage((uchar*)(display_ref_r.data), display_ref_r.cols, display_ref_r.rows, display_ref_r.step, QImage::Format_RGB888);
            ui->ref_red_view_zone->setPixmap(QPixmap::fromImage(display_r));
        }

        view_zone_step->setMapping(calibrate_timer, 2);
        calibrate_timer->start();


    }
    else if(step==2){

          //set threshold
        Mat tmp_dual_1 = dual_1.current_frame.clone();
        Mat tmp_dual_2 = dual_2.current_frame.clone();

      if(dual_1.side==1){
          //1號是右眼

        Calibrate.set_image_by_dual_camera(tmp_dual_2(dual_2.bounding_rect), tmp_dual_1(dual_1.bounding_rect));
      }else {

       Calibrate.set_image_by_dual_camera(tmp_dual_1(dual_1.bounding_rect), tmp_dual_2(dual_2.bounding_rect));
      }


        my_view_zone.rg_ratio_threshold_left = Calibrate.get_RG_Ratio_single_color(0, false); //compare with red
        my_view_zone.rg_ratio_threshold_right =  Calibrate.get_RG_Ratio_single_color(1, false); //compare with green


        qDebug()<< "thresh ini"<<my_view_zone.rg_ratio_threshold_left<<my_view_zone.rg_ratio_threshold_right;




        if(calibrate_mode==1){
            AUO3D_imagePath_LR(PC_image_path(G),PC_image_path(R));
        }else if(calibrate_mode==0){
            Second_Monitor_mode(G_R);
        }

        view_zone_step->setMapping(calibrate_timer, 3);
        calibrate_timer->start(); }
    else if(step==3){
        calibrate_timer->setInterval(800);
        Mat tmp_dual_1 = dual_1.current_frame.clone();
        Mat tmp_dual_2 = dual_2.current_frame.clone();


        if(dual_1.side==1){
            //1號是右眼                             green  in left compare red                         red  in right compare green
          Calibrate.set_image_by_dual_camera(tmp_dual_2(dual_2.bounding_rect), tmp_dual_1(dual_1.bounding_rect));
        }else {


         Calibrate.set_image_by_dual_camera(tmp_dual_1(dual_1.bounding_rect), tmp_dual_2(dual_2.bounding_rect));
        }


         double rg_ratio_left = Calibrate.get_RG_Ratio_single_color(0, false);
         double rg_ratio_right = Calibrate.get_RG_Ratio_single_color(1, false);


        //my_view_zone.rg_ratio_threshold_left =  my_view_zone.rg_ratio_threshold_left + (rg_ratio_left -  my_view_zone.rg_ratio_threshold_left ) *0.25;
        //my_view_zone.rg_ratio_threshold_right =  my_view_zone.rg_ratio_threshold_right+(rg_ratio_right -  my_view_zone.rg_ratio_threshold_right) *0.25;
        my_view_zone.rg_ratio_threshold_left =  (rg_ratio_left -  my_view_zone.rg_ratio_threshold_left ) *RG_THRESHOLD ;
        my_view_zone.rg_ratio_threshold_right =  (rg_ratio_right -  my_view_zone.rg_ratio_threshold_right) *RG_THRESHOLD ;

        qDebug()<< "thresh rg"<<rg_ratio_left<<rg_ratio_right;
        qDebug()<< "thresh final"<<my_view_zone.rg_ratio_threshold_left<<my_view_zone.rg_ratio_threshold_right;


        my_view_zone.right_view_zone_length =  my_view_zone.right_view_zone_length + my_view_zone.view_adjust_step;
        my_view_zone.tmp_xoff_len = my_calibrate.xoff_lens + my_view_zone.right_view_zone_length;

        if(calibrate_mode==0){         
             AUO3D_FPGA_SendData(PanelData::XOFF_LENS, &my_view_zone.tmp_xoff_len);
        }else if(calibrate_mode==1){          
             AUO3D_SendData(PanelData::XOFF_LENS, &my_view_zone.tmp_xoff_len);
        }

        if(calibrate_mode==1){
             AUO3D_imagePath_LR(PC_image_path(R),PC_image_path(G));
        }else if(calibrate_mode==0){
             Second_Monitor_mode(R_G);
        }
        view_zone_step->setMapping(calibrate_timer, 4);
        calibrate_timer->start();
    }
    else if(step==4){

        Mat tmp_dual_1 = dual_1.current_frame.clone();
        Mat tmp_dual_2 = dual_2.current_frame.clone();
        if(dual_1.side==1){
            //1號是右眼
          Calibrate.set_image_by_dual_camera(tmp_dual_2(dual_2.bounding_rect), tmp_dual_1(dual_1.bounding_rect));
        }else {

         Calibrate.set_image_by_dual_camera(tmp_dual_1(dual_1.bounding_rect), tmp_dual_2(dual_2.bounding_rect));
        }

         double rg_ratio_right_roi =  Calibrate.get_RG_Ratio_single_color_roi(1);
         double rg_ratio_left_roi =  Calibrate.get_RG_Ratio_single_color_roi(0);


         qDebug()<< "rg start"<< rg_ratio_left_roi<<rg_ratio_right_roi ;

         ui->view_zone_ratio_left->setText(QString::number(rg_ratio_left_roi));
         ui->view_zone_ratio_right->setText(QString::number(rg_ratio_right_roi));

         if(my_view_zone.rg_ratio_threshold_left > rg_ratio_left_roi && my_view_zone.rg_ratio_threshold_right > rg_ratio_right_roi){
            //not search yet
             my_view_zone.right_view_zone_length = my_view_zone.right_view_zone_length + my_view_zone.view_adjust_step;
             my_view_zone.tmp_xoff_len = my_calibrate.xoff_lens + my_view_zone.right_view_zone_length;
             if(calibrate_mode==0){
                  AUO3D_FPGA_SendData(PanelData::XOFF_LENS, &my_view_zone.tmp_xoff_len);
             }else if(calibrate_mode==1){
                  AUO3D_SendData(PanelData::XOFF_LENS, &my_view_zone.tmp_xoff_len);
             }
             view_zone_step->setMapping(calibrate_timer,4);
             calibrate_timer->start();
         }
         else{
             //set back
             my_view_zone.view_zone = my_view_zone.right_view_zone_length;
             my_view_zone.right_view_zone_length = 0;
             ui->view_zone_value->setText(QString::number( my_view_zone.view_zone));

             my_view_zone.left_view_zone_length = my_view_zone.left_view_zone_length +  my_view_zone.view_adjust_step;
             my_view_zone.tmp_xoff_len = my_calibrate.xoff_lens - my_view_zone.left_view_zone_length;
             if(calibrate_mode==0){
                  AUO3D_FPGA_SendData(PanelData::XOFF_LENS, &my_view_zone.tmp_xoff_len);
             }else if(calibrate_mode==1){
                  AUO3D_SendData(PanelData::XOFF_LENS, &my_view_zone.tmp_xoff_len);
             }
             view_zone_step->setMapping(calibrate_timer, 5);
             calibrate_timer->start();
         }
    }
    else if(step==5){

        Mat tmp_dual_1 = dual_1.current_frame.clone();
        Mat tmp_dual_2 = dual_2.current_frame.clone();
        if(dual_1.side==1){
            //1號是右眼
          Calibrate.set_image_by_dual_camera(tmp_dual_2(dual_2.bounding_rect), tmp_dual_1(dual_1.bounding_rect));
        }else {
         Calibrate.set_image_by_dual_camera(tmp_dual_1(dual_1.bounding_rect), tmp_dual_2(dual_2.bounding_rect));
        }

        double rg_ratio_right_roi =  Calibrate.get_RG_Ratio_single_color_roi(1);
        double rg_ratio_left_roi =  Calibrate.get_RG_Ratio_single_color_roi(0);


        ui->view_zone_ratio_left->setText(QString::number(rg_ratio_left_roi));
        ui->view_zone_ratio_right->setText(QString::number(rg_ratio_right_roi));


        if(my_view_zone.rg_ratio_threshold_left > rg_ratio_left_roi && my_view_zone.rg_ratio_threshold_right > rg_ratio_right_roi){
            //not search yet
             my_view_zone.left_view_zone_length = my_view_zone.left_view_zone_length +  my_view_zone.view_adjust_step;
              my_view_zone.tmp_xoff_len = my_calibrate.xoff_lens - my_view_zone.left_view_zone_length;
             if(calibrate_mode==0){
                  AUO3D_FPGA_SendData(PanelData::XOFF_LENS, &my_view_zone.tmp_xoff_len);
             }else if(calibrate_mode==1){
                  AUO3D_SendData(PanelData::XOFF_LENS, &my_view_zone.tmp_xoff_len);
             }
             view_zone_step->setMapping(calibrate_timer, 5);
             calibrate_timer->start();

        }else{

            my_view_zone.view_zone = my_view_zone.view_zone + my_view_zone.left_view_zone_length ;
            my_view_zone.left_view_zone_length = 0;
            ui->view_zone_value->setText(QString::number(my_view_zone.view_zone));

            if(calibrate_mode==0){
                 AUO3D_FPGA_SendData(PanelData::XOFF_LENS, &my_calibrate.xoff_lens);
            }else if(calibrate_mode==1){
                 AUO3D_SendData(PanelData::XOFF_LENS, &my_calibrate.xoff_lens);
            }

            /*
            if(calibrate_mode==0){
                 AUO3D_FPGA_SendData(PanelData::XOFF_LENS, &parameters.x_off_lens);
            }else if(calibrate_mode==1){
                 AUO3D_SendData(PanelData::XOFF_LENS, &parameters.x_off_lens);
            }
            */

            my_calibrate.view_zone = my_view_zone.view_zone;

            my_calibrate.view_zone_mm = (my_calibrate.view_zone * parameters.pixel_size/3)/parameters.IT_default * 1.52 * parameters.OVD;
            ui->view_zone_value->setText(QString::number(my_calibrate.view_zone_mm)+" mm");

            disconnect(calibrate_timer, SIGNAL(timeout()), view_zone_step, SLOT(map()));
            ui->start_calibrate_view_zone->setText(QString::fromLocal8Bit("評估完成 點擊重新校正"));
            ui->start_calibrate_view_zone->setEnabled(true);
            ui->Calibration_Function_Box->setEnabled(true);
            ui->calibrate_next->show();
            dual_1.stop_update_roi=false;
            dual_2.stop_update_roi=false;
            //calibrate_timer->setInterval(CALIBRATE_UPDATE_TIME);
            ui->to_view_zone->setStyleSheet("QPushButton{background-color: rgb(46, 125, 161);}");
            ui->to_view_zone->setEnabled(true);

            if(calibrate_mode==0){
                 my_calibrate.cam_off_x = fpga_eye_tracking.x *-1;
                 my_calibrate.cam_off_y = fpga_eye_tracking.y *-1;
                 my_calibrate.cam_off_vd = (fpga_eye_tracking.z - parameters.OVD)*-1;
                 AUO3D_FPGA_SendData(PanelData::CAM_OFT_X, &my_calibrate.cam_off_x);
                 AUO3D_FPGA_SendData(PanelData::CAM_OFT_Y, &my_calibrate.cam_off_y);
                 AUO3D_FPGA_SendData(PanelData::CAM_OFT_Z, &my_calibrate.cam_off_vd);


            }else if(calibrate_mode==1){
                 my_calibrate.cam_off_x = pc_eye_tracking.x *-1;
                 my_calibrate.cam_off_y = pc_eye_tracking.y *-1;
                 my_calibrate.cam_off_vd = (pc_eye_tracking.z - parameters.OVD)*-1;
                 AUO3D_SendData(PanelData::CAM_OFT_X, &my_calibrate.cam_off_x);
                 AUO3D_SendData(PanelData::CAM_OFT_Y, &my_calibrate.cam_off_y);
                 AUO3D_SendData(PanelData::CAM_OFT_Z, &my_calibrate.cam_off_vd);
            }


        }

    }


    //gray scale test
    else if(step==6){
        if (!file_gray.open(QIODevice::WriteOnly | QIODevice::Text))
        {
            qDebug() << "Failed to open the file for writing: " << file.errorString();

        }

        if(gray_count<255){
            QPixmap img;
            QString qstr = "C://Users/auott/Desktop/3D_Calibration/gray_255/gray_";
            img.load(qstr+QString::number(gray_count)+".png");
            gray_count++;
            img = img.scaled(3840, 2160);
            second_monitor.label->setPixmap(img);
            if (!second_monitor.widget->isVisible()) {
                second_monitor.widget->show();
            }
            view_zone_step->setMapping(calibrate_timer, 7);
            calibrate_timer->start();
        }

    }
    else if(step ==7){

        if(gray_count<255){
            Mat dual_1_roi = dual_1.current_frame(dual_1.bounding_rect).clone();
            Mat dual_2_roi = dual_2.current_frame(dual_2.bounding_rect).clone();
            int* gray_data =NULL;
            int* gray_data2 =NULL;
            double gray_mean = 0.0;
            double gray_mean2 = 0.0;

            gray_data = Calibrate.find_gray(dual_1_roi,&gray_mean);
            gray_data2 = Calibrate.find_gray(dual_2_roi,&gray_mean2);

            out<<gray_mean<<",";
            out<<gray_data[0]<<",";
            out<<gray_data[1]<<",";
            out<<gray_data[2]<<",";

            out<<gray_mean2<<",";
            out<<gray_data2[0]<<",";           
            out<<gray_data2[1]<<",";
            out<<gray_data2[2]<<"\n";

            delete gray_data;
            delete gray_data2;

            QPixmap img;
            QString qstr = "C://Users/auott/Desktop/3D_Calibration/gray_255/gray_";
            img.load(qstr+QString::number(gray_count)+".png");
            img = img.scaled(3840, 2160);
            second_monitor.label->setPixmap(img);
            if (!second_monitor.widget->isVisible()) {
                second_monitor.widget->show();
            }

            gray_count++;

            view_zone_step->setMapping(calibrate_timer, 7);
            calibrate_timer->start();

        }else{
            disconnect(calibrate_timer, SIGNAL(timeout()), view_zone_step, SLOT(map()));
            ui->start_calibrate_view_zone->setText(QString::fromLocal8Bit("評估完成 點擊重新校正"));
            ui->start_calibrate_view_zone->setEnabled(true);
            ui->Calibration_Function_Box->setEnabled(true);
            ui->calibrate_next->show();
            dual_1.stop_update_roi=false;
            dual_2.stop_update_roi=false;
            //calibrate_timer->setInterval(CALIBRATE_UPDATE_TIME);
            ui->to_view_zone->setStyleSheet("QPushButton{background-color: rgb(46, 125, 161);}");
            ui->to_view_zone->setEnabled(true);
            file_gray.close();
        }

    }


    //Cross Talk Test
    else if(step==8){
        //set ref
        if(dual_1.side==1){
            //1 is right
             tmp_right_black = dual_1.current_frame(dual_1.bounding_rect).clone();
             tmp_left_white = dual_2.current_frame(dual_2.bounding_rect).clone();
        }else{
            tmp_right_black = dual_2.current_frame(dual_2.bounding_rect).clone();
            tmp_left_white = dual_1.current_frame(dual_1.bounding_rect).clone();
        }


        if(calibrate_mode==1){
            AUO3D_imagePath_LR(PC_image_path(B),PC_image_path(W));
        }else if(calibrate_mode==0){
            Second_Monitor_mode(B_W);
        }
        view_zone_step->setMapping(calibrate_timer, 9);
        calibrate_timer->start();

    }else if(step==9){
        if(dual_1.side==1){
            //1 is right
            tmp_right_white = dual_1.current_frame(dual_1.bounding_rect).clone();
            tmp_left_black = dual_2.current_frame(dual_2.bounding_rect).clone();
        }else{
            tmp_right_white = dual_2.current_frame(dual_2.bounding_rect).clone();
            tmp_left_black = dual_1.current_frame(dual_1.bounding_rect).clone();
        }

        if(calibrate_mode==1){
            AUO3D_imagePath_LR(PC_image_path(W),PC_image_path(B));
        }else if(calibrate_mode==0){
            Second_Monitor_mode(W_B);
        }
        view_zone_step->setMapping(calibrate_timer, 10);
        calibrate_timer->start();

    }
    else if(step==10){
        double right_crosstalk_mean=0.0;
        double left_crosstalk_mean=0.0;
        double ratio_step=0.05;
        Mat right_crosstalk_map = Calibrate.get_crosstalk_map(tmp_right_white,tmp_right_black,&right_crosstalk_mean);
        Mat left_crosstalk_map = Calibrate.get_crosstalk_map(tmp_left_white,tmp_left_black,&right_crosstalk_mean);

        if(right_crosstalk_map.empty() || left_crosstalk_map.empty()){
            qDebug()<<"error";
            return;
        }
        Mat right_crosstalk_img = Calibrate.draw_crosstalk(tmp_right_black,right_crosstalk_map,ratio_step);
        Mat left_crosstalk_img = Calibrate.draw_crosstalk(tmp_left_white,left_crosstalk_map,ratio_step);
        if(right_crosstalk_img.empty() || left_crosstalk_img.empty()){
            qDebug()<<"error";
            return;
        }
        Mat result_r;
        Mat result_l;

        cv::vconcat(right_crosstalk_img,tmp_right_black, result_r );
        cv::vconcat(left_crosstalk_img,tmp_left_white, result_l );

        if(my_view_zone.tmp_xoff_len>0){
            imwrite("./crosstalk/Right_1_"+QString::number(my_view_zone.tmp_xoff_len).toStdString()+".jpg" , result_r);
            imwrite("./crosstalk/left/Left_1_"+QString::number(my_view_zone.tmp_xoff_len).toStdString()+".jpg" ,result_l);
        }else{
            imwrite("./crosstalk/Right_0_"+QString::number(my_view_zone.tmp_xoff_len+1).toStdString()+".jpg" ,result_r );
            imwrite("./crosstalk/left/Left_0_"+QString::number(my_view_zone.tmp_xoff_len+1).toStdString()+".jpg" ,result_l);
        }


        if(my_view_zone.tmp_xoff_len < my_calibrate.xoff_lens+2 && my_view_zone.tmp_xoff_len > my_calibrate.xoff_lens-0.1){
            my_view_zone.tmp_xoff_len =  my_view_zone.tmp_xoff_len+0.1;
            if(calibrate_mode==0){
                AUO3D_FPGA_SendData(PanelData::XOFF_LENS, &my_view_zone.tmp_xoff_len);
            }else if(calibrate_mode==1){
                AUO3D_SendData(PanelData::XOFF_LENS, &my_view_zone.tmp_xoff_len);

            }
            view_zone_step->setMapping(calibrate_timer, 8);
            calibrate_timer->start();
        }else if(my_view_zone.tmp_xoff_len>my_calibrate.xoff_lens+2){
            my_view_zone.tmp_xoff_len =my_calibrate.xoff_lens-0.1;
            if(calibrate_mode==0){
                AUO3D_FPGA_SendData(PanelData::XOFF_LENS, &my_view_zone.tmp_xoff_len);
            }else if(calibrate_mode==1){
                AUO3D_SendData(PanelData::XOFF_LENS, &my_view_zone.tmp_xoff_len);
            }
            view_zone_step->setMapping(calibrate_timer, 8);
            calibrate_timer->start();
        }else if(my_view_zone.tmp_xoff_len>my_calibrate.xoff_lens-2.1&& my_view_zone.tmp_xoff_len < my_calibrate.xoff_lens){
            my_view_zone.tmp_xoff_len = my_view_zone.tmp_xoff_len -0.1;
            if(calibrate_mode==0){
                AUO3D_FPGA_SendData(PanelData::XOFF_LENS, &my_view_zone.tmp_xoff_len);
            }else if(calibrate_mode==1){
                AUO3D_SendData(PanelData::XOFF_LENS, &my_view_zone.tmp_xoff_len);
            }
            view_zone_step->setMapping(calibrate_timer, 8);
            calibrate_timer->start();

        }else if(my_view_zone.tmp_xoff_len<my_calibrate.xoff_lens-2){

            my_view_zone.tmp_xoff_len = my_view_zone.tmp_xoff_len;
            if(calibrate_mode==0){
                AUO3D_FPGA_SendData(PanelData::XOFF_LENS, &my_view_zone.tmp_xoff_len);
            }else if(calibrate_mode==1){
                AUO3D_SendData(PanelData::XOFF_LENS, &my_view_zone.tmp_xoff_len);

            }
            disconnect(calibrate_timer, SIGNAL(timeout()), view_zone_step, SLOT(map()));
            ui->start_calibrate_view_zone->setText(QString::fromLocal8Bit("評估完成 點擊重新校正"));
            ui->start_calibrate_view_zone->setEnabled(true);
            ui->Calibration_Function_Box->setEnabled(true);
            ui->calibrate_next->show();
            dual_1.stop_update_roi=false;
            dual_2.stop_update_roi=false;
            //calibrate_timer->setInterval(CALIBRATE_UPDATE_TIME);
            ui->to_view_zone->setStyleSheet("QPushButton{background-color: rgb(46, 125, 161);}");
            ui->to_view_zone->setEnabled(true);

        }
    }

    // Cross Talk Verifi
    else if(step==11){

        if(dual_1.side==1){
            //1 is right
             tmp_right_black = dual_1.current_frame(dual_1.bounding_rect).clone();
             tmp_left_white = dual_2.current_frame(dual_2.bounding_rect).clone();
        }else{
            tmp_right_black = dual_2.current_frame(dual_2.bounding_rect).clone();
            tmp_left_white = dual_1.current_frame(dual_1.bounding_rect).clone();
        }
        if(calibrate_mode==1){
            AUO3D_imagePath_LR(PC_image_path(B),PC_image_path(W));
        }else if(calibrate_mode==0){
            Second_Monitor_mode(B_W);
        }
        view_zone_step->setMapping(calibrate_timer, 12);
        calibrate_timer->start();

    }
    else if(step==12){

        if(dual_1.side==1){
            //1 is right
            tmp_right_white = dual_1.current_frame(dual_1.bounding_rect).clone();
            tmp_left_black = dual_2.current_frame(dual_2.bounding_rect).clone();
        }else{
            tmp_right_white = dual_2.current_frame(dual_2.bounding_rect).clone();
            tmp_left_black = dual_1.current_frame(dual_1.bounding_rect).clone();
        }

        if(calibrate_mode==1){
            AUO3D_imagePath_LR(PC_image_path(W),PC_image_path(B));
        }else if(calibrate_mode==0){
            Second_Monitor_mode(W_B);
        }

        if(my_view_zone.search_dir_pos==true){
            view_zone_step->setMapping(calibrate_timer, 13);
            calibrate_timer->start();
        }else{
            view_zone_step->setMapping(calibrate_timer, 14);
            calibrate_timer->start();
        }

    }
    else if(step==13){

        double right_crosstalk_mean;
        double left_crosstalk_mean;
        double current_crosstalk_right=0.0;
        double current_crosstalk_left=0.0;

        double ratio_step=0.1;
        Mat right_crosstalk_map = Calibrate.get_crosstalk_map(tmp_right_white,tmp_right_black,&right_crosstalk_mean);
        Mat left_crosstalk_map = Calibrate.get_crosstalk_map(tmp_left_white,tmp_left_black,&left_crosstalk_mean);

        if(right_crosstalk_map.empty() || left_crosstalk_map.empty()){
            qDebug()<<"error";
            return;
        }

        //Record image
        Mat right_crosstalk_img = Calibrate.draw_crosstalk(tmp_right_black,right_crosstalk_map,ratio_step);
        Mat left_crosstalk_img = Calibrate.draw_crosstalk(tmp_left_black,left_crosstalk_map,ratio_step);
        if(right_crosstalk_img.empty() || left_crosstalk_img.empty()){
            qDebug()<<"error";
            return;
        }
        Mat result_r;
        Mat result_l;
        Mat view;
        cv::vconcat(right_crosstalk_img,tmp_right_black, result_r );
        cv::vconcat(left_crosstalk_img,tmp_left_black, result_l );
        cv::resize(result_l,result_l, result_r.size());
        cv::hconcat( result_l, result_r, view );

        //Record End

        int right = Calibrate.find_view_edge(right_crosstalk_map,0.1,0.05,1.0,true);
        int left = Calibrate.find_view_edge(left_crosstalk_map,0.1,0.05,1.0,false);


        if(right ==1 || left ==1){

            if(!find_positive_edge){
                 find_positive_edge = true;
                 positive_view_length = my_view_zone.right_view_zone_length - my_view_zone.view_adjust_step;
             }

         }
        //qDebug()<<tmp_crosstalk_right;

        tmp_crosstalk_right = Calibrate.current_cross_talk_right;
        tmp_crosstalk_left = Calibrate.current_cross_talk_left;

        positive.push_back({view,tmp_left_black,tmp_left_white,tmp_right_black,tmp_right_white,tmp_crosstalk_right,tmp_crosstalk_left});

        if(count_view!=0){
            //in Center 100% area as total pixels , IF number of (crosstalk ratio > 10%  pixels ) > 5% of total pixels => find edge
            // 1 is not find edge yet
            //find positive side
            my_view_zone.right_view_zone_length = my_view_zone.right_view_zone_length + my_view_zone.view_adjust_step;
            my_view_zone.tmp_xoff_len = my_calibrate.xoff_lens + my_view_zone.right_view_zone_length;
            if(calibrate_mode==0){
                 AUO3D_FPGA_SendData(PanelData::XOFF_LENS, &my_view_zone.tmp_xoff_len);
            }else if(calibrate_mode==1){
                 AUO3D_SendData(PanelData::XOFF_LENS, &my_view_zone.tmp_xoff_len);
            }
            count_view--;
            view_zone_step->setMapping(calibrate_timer, 11);
            calibrate_timer->start();

        }
        else if(count_view == 0){
            // 2 is find edge
            //go to negtive side
            my_view_zone.search_dir_pos = false;
            my_view_zone.view_zone = my_view_zone.right_view_zone_length - my_view_zone.view_adjust_step;
            my_view_zone.right_view_zone_length = 0;
            my_view_zone.left_view_zone_length = my_view_zone.view_adjust_step;
            my_view_zone.tmp_xoff_len = my_calibrate.xoff_lens-my_view_zone.view_adjust_step;
            if(calibrate_mode==0){
                 AUO3D_FPGA_SendData(PanelData::XOFF_LENS, &my_view_zone.tmp_xoff_len);
            }else if(calibrate_mode==1){
                 AUO3D_SendData(PanelData::XOFF_LENS, &my_view_zone.tmp_xoff_len);
            }
            count_view = 15;
            view_zone_step->setMapping(calibrate_timer,11);
            calibrate_timer->start();
        }
    }
   else if(step ==14){

        double right_crosstalk_mean;
        double left_crosstalk_mean;


        double ratio_step=0.1;
        Mat right_crosstalk_map = Calibrate.get_crosstalk_map(tmp_right_white,tmp_right_black,&right_crosstalk_mean);
        Mat left_crosstalk_map = Calibrate.get_crosstalk_map(tmp_left_white,tmp_left_black,&right_crosstalk_mean);

        if(right_crosstalk_map.empty() || left_crosstalk_map.empty()){
            qDebug()<<"error";
            return;
        }

        //Record image
        Mat right_crosstalk_img = Calibrate.draw_crosstalk(tmp_right_black,right_crosstalk_map,ratio_step);
        Mat left_crosstalk_img = Calibrate.draw_crosstalk(tmp_left_black,left_crosstalk_map,ratio_step);
        if(right_crosstalk_img.empty() || left_crosstalk_img.empty()){
            qDebug()<<"error";
            return;
        }
        Mat result_r;
        Mat result_l;
        Mat view;
        cv::vconcat(right_crosstalk_img,tmp_right_black, result_r );
        cv::vconcat(left_crosstalk_img,tmp_left_black, result_l );
        cv::resize(result_l,result_l, result_r.size());
        cv::hconcat(result_l,result_r, view );

        image_num++;

            // Record End

        int right = Calibrate.find_view_edge(right_crosstalk_map,0.1,0.05,1.0,true);
        int left = Calibrate.find_view_edge(left_crosstalk_map,0.1,0.05,1.0,false);


       if(right ==1 || left ==1){
           if(!find_negative_edge){
               find_negative_edge = true;
               negative_view_length = my_view_zone.left_view_zone_length - my_view_zone.view_adjust_step;
           }

        }

           tmp_crosstalk_right = Calibrate.current_cross_talk_right;
           tmp_crosstalk_left = Calibrate.current_cross_talk_left;

           negative.push_back({view,tmp_left_black,tmp_left_white,tmp_right_black,tmp_right_white,tmp_crosstalk_right,tmp_crosstalk_left });


            if(count_view!=0){
                //find negtive side
                my_view_zone.left_view_zone_length = my_view_zone.left_view_zone_length + my_view_zone.view_adjust_step;
                my_view_zone.tmp_xoff_len = my_calibrate.xoff_lens - my_view_zone.left_view_zone_length;
                if(calibrate_mode==0){
                     AUO3D_FPGA_SendData(PanelData::XOFF_LENS, &my_view_zone.tmp_xoff_len);
                }else if(calibrate_mode==1){
                     AUO3D_SendData(PanelData::XOFF_LENS, &my_view_zone.tmp_xoff_len);
                }
                count_view -- ;
                view_zone_step->setMapping(calibrate_timer, 11);
                calibrate_timer->start();
            }
            else if(count_view==0){

                std::reverse(negative.begin(), negative.end());
                negative.insert(negative.end(),positive.begin(),positive.end());

                QString filePath = "./crosstalk/view_zone.txt" ;
                QFile file(filePath);

                 double view_zone_in_mm = (positive_view_length + negative_view_length) / 6.37 * 130;

                 if (file.open(QIODevice::WriteOnly | QIODevice::Text)){
                     QTextStream stream(&file);
                     stream<<"\n"<<"[View Zone Data]\n";
                     stream <<"View Zone in Xoff Lens :"<< QString::number( positive_view_length + negative_view_length )<<"\n";
                      stream <<"View Zone in mm :"<< QString::number(view_zone_in_mm)<<"\n\n\n";
                     stream << "View Zone in positive side length :"<< QString::number(positive_view_length)<<"\n";
                     stream << "image number in positive edge  :"<< QString::number(positive_view_length/0.1+1)<<"\n";
                     stream << "View Zone in negative side length :"<< QString::number(negative_view_length)<<"\n";
                     stream << "image number in negative edge  :"<< QString::number(negative_view_length/0.1)<<"\n";

                     for(int i=0 ;i<negative.size();i++ ){
                         imwrite("./crosstalk/right_origin/"+QString::number(i).toStdString()+".jpg" ,negative.at(i).rb);
                         //imwrite("./crosstalk/left_origin/"+QString::number(i).toStdString()+".jpg" ,negative.at(i).rw);
                         imwrite("./crosstalk/left_origin/"+QString::number(i).toStdString()+".jpg" ,negative.at(i).lb);
                         //imwrite("./crosstalk/lw_"+QString::number(i).toStdString()+".jpg" ,negative.at(i).lw);
                         imwrite("./crosstalk/full_view/"+QString::number(i).toStdString()+".jpg" ,negative.at(i).full_image);
                         stream << QString::number(i)+"_right:"+ QString::number(negative.at(i).cross_talk_right)<<"\n";
                         stream << QString::number(i)+"_left:"+ QString::number(negative.at(i).cross_talk_left)<<"\n";
                     }


                     file.close();
                 }



                my_view_zone.search_dir_pos = true;
                my_view_zone.view_zone =  my_view_zone.view_zone + my_view_zone.left_view_zone_length -  my_view_zone.view_adjust_step;



                my_view_zone.left_view_zone_length = 0;
                my_calibrate.view_zone =  my_view_zone.view_zone;

                ui->view_zone_value->setText(QString::number(my_calibrate.view_zone));
                negative.clear();
                positive.clear();

                my_view_zone.tmp_xoff_len = my_calibrate.xoff_lens;
                if(calibrate_mode==0){
                     AUO3D_FPGA_SendData(PanelData::XOFF_LENS, &my_view_zone.tmp_xoff_len);
                }else if(calibrate_mode==1){
                     AUO3D_SendData(PanelData::XOFF_LENS, &my_view_zone.tmp_xoff_len);
                }


                //end Calibration
                disconnect(calibrate_timer, SIGNAL(timeout()), view_zone_step, SLOT(map()));
                ui->start_calibrate_view_zone->setText(QString::fromLocal8Bit("評估完成 點擊重新校正"));
                ui->start_calibrate_view_zone->setEnabled(true);
                ui->Calibration_Function_Box->setEnabled(true);
                ui->calibrate_next->show();
                dual_1.stop_update_roi=false;
                dual_2.stop_update_roi=false;
                ui->to_view_zone->setStyleSheet("QPushButton{background-color: rgb(46, 125, 161);}");
                ui->to_view_zone->setEnabled(true);
            }


    }



}

//-----------------------------------------------------Page 7 Function(VD/IT)
void MainWindow::start_vd(){

    if(dual_1.is_contour&&dual_2.is_contour){

        //parameters.fov_h_max = ui->h_max->text().toDouble();

        ui->vd_warn->setText(QString::fromLocal8Bit(""));
        dual_1.stop_update_roi=true;
        dual_2.stop_update_roi=true;

        connect(calibrate_timer, SIGNAL(timeout()),  vd_it_step, SLOT(map()));
        ui->start_calibrate_vd->setText(QString::fromLocal8Bit("校正中...."));
        ui->start_calibrate_vd->setEnabled(false);
        ui->Calibration_Function_Box->setEnabled(false);

        if(calibrate_mode==0){
            //Second_Monitor_mode(R_R);
            AUO3D_FPGA_SendData(PanelData::X, &my_vd_it.set_x);
            AUO3D_FPGA_SendData(PanelData::Y, &my_vd_it.set_y);
            AUO3D_FPGA_SendData(PanelData::VD,&my_vd_it.set_z);
            AUO3D_FPGA_SendData(PanelData::A, &parameters.A);
            AUO3D_FPGA_SendData(PanelData::B, &parameters.B);
            AUO3D_FPGA_SendData(PanelData::C, &parameters.C);
            AUO3D_FPGA_SendData(PanelData::E1,&parameters.E1);
            AUO3D_FPGA_SendData(PanelData::E2,&parameters.E2);


        }else if(calibrate_mode==1){
            AUO3D_SendData(PanelData::X, &my_vd_it.set_x);
            AUO3D_SendData(PanelData::Y, &my_vd_it.set_y);
            AUO3D_SendData(PanelData::VD,&my_vd_it.set_z);
            AUO3D_SendData(PanelData::A, &parameters.A);
            AUO3D_SendData(PanelData::B, &parameters.B);
            AUO3D_SendData(PanelData::C, &parameters.C);
            AUO3D_SendData(PanelData::E1,&parameters.E1);
            AUO3D_SendData(PanelData::E2,&parameters.E2);
        }


        calibrate_timer->setInterval(1000);
        vd_it_step->setMapping(calibrate_timer, 8);
        calibrate_timer->start();
    }

 /*
    if(calibrate_mode==0){
        x = fpga_eye_tracking.x+my_calibrate.cam_off_x;
        y=fpga_eye_tracking.y+my_calibrate.cam_off_y;
        vd = fpga_eye_tracking.z+my_calibrate.cam_off_vd;


    }else if(calibrate_mode==1){
        x = pc_eye_tracking.x+my_calibrate.cam_off_x;
        y= pc_eye_tracking.y+my_calibrate.cam_off_y;
        vd = pc_eye_tracking.z+my_calibrate.cam_off_vd;
    }
    */
 /*
            if(dual_1.is_contour&&dual_2.is_contour){
                 ui->vd_warn->setText(QString::fromLocal8Bit(""));
                 dual_1.stop_update_roi=true;
                 dual_2.stop_update_roi=true;


                 if(my_vd_it.current_stage==0){
                     connect(calibrate_timer, SIGNAL(timeout()),  vd_it_step, SLOT(map()));
                 }

                 my_vd_it.is_calibrating=true;
                 ui->start_calibrate_vd->setText(QString::fromLocal8Bit("校正中...."));
                 ui->start_calibrate_vd->setEnabled(false);
                 ui->Calibration_Function_Box->setEnabled(false);

                 my_vd_it.tmp_C = 0;
                 my_vd_it.single_search=false;
                 my_vd_it.single_search_is_add=false;
                 my_vd_it.rg_ratio_threshold_left =0.0;
                 my_vd_it.rg_ratio_threshold_right = 0.0;
                 my_vd_it.add_move_length = 0;
                 my_vd_it.dec_move_length = 0;
                 my_vd_it.rg_ratio_tmp_left=0;
                 my_vd_it.rg_ratio_tmp_right=0;

                 if(calibrate_mode==0){
                     Second_Monitor_mode(R_R);

                     if(my_vd_it.current_stage==0){
                         my_vd_it.A = parameters.A;
                         my_vd_it.B = parameters.B;
                         my_vd_it.C = parameters.C;
                         AUO3D_FPGA_SendData(PanelData::A,&my_vd_it.A);
                         AUO3D_FPGA_SendData(PanelData::B,&my_vd_it.B);
                         AUO3D_FPGA_SendData(PanelData::C,&my_vd_it.C);
                     }else if(my_vd_it.current_stage==3){
                         my_vd_it.E1 = my_calibrate.e1;
                         my_vd_it.E2 = my_calibrate.e2;
                         AUO3D_FPGA_SendData(PanelData::E1,&my_vd_it.E1);
                         AUO3D_FPGA_SendData(PanelData::E2,&my_vd_it.E2);
                     }

                 }else if(calibrate_mode==1){
                     AUO3D_imagePath_LR(PC_image_path(R),PC_image_path(R));

                     if(my_vd_it.current_stage==0){
                         my_vd_it.A = parameters.A;
                         my_vd_it.B = parameters.B;
                         my_vd_it.C = parameters.C;
                         AUO3D_SendData(PanelData::A,&my_vd_it.A);
                         AUO3D_SendData(PanelData::B,&my_vd_it.B);
                         AUO3D_SendData(PanelData::C,&my_vd_it.C);
                     }else if(my_vd_it.current_stage==3){
                         my_vd_it.E1 = my_calibrate.e1;
                         my_vd_it.E2 = my_calibrate.e2;
                         AUO3D_SendData(PanelData::E1,&my_vd_it.E1);
                         AUO3D_SendData(PanelData::E2,&my_vd_it.E2);
                     }
                 }


                 calibrate_timer->setInterval(1200);
                 vd_it_step->setMapping(calibrate_timer, 0);
                 calibrate_timer->start();

            }else{
                 ui->vd_warn->setText(QString::fromLocal8Bit("未偵測到ROI"));
            }

*/
}

void MainWindow::calibrate_vd(int step){

    if(step==0){
        //set threshold
      Mat tmp_dual_1 = dual_1.current_frame.clone();
      Mat tmp_dual_2 = dual_2.current_frame.clone();
    if(dual_1.side==1){
        //1號是右眼
      Calibrate.set_image_by_dual_camera(tmp_dual_2(dual_2.bounding_rect), tmp_dual_1(dual_1.bounding_rect));
    }else {
     Calibrate.set_image_by_dual_camera(tmp_dual_1(dual_1.bounding_rect), tmp_dual_2(dual_2.bounding_rect));
    }

        //set ref red
        if(dual_1.side==1){

            Calibrate.set_ref_by_camera(dual_2.current_frame(dual_2.bounding_rect),'R');
            int *ref_r = new int[3];
            ref_r = Calibrate.get_ref_rgb('R');

            Mat display_ref_r(ui->ref_red_vd->height(), ui->ref_red_vd->width(), CV_8UC3, Scalar(ref_r[0], ref_r[1], ref_r[2]));
            QImage display_r = QImage((uchar*)(display_ref_r.data), display_ref_r.cols, display_ref_r.rows, display_ref_r.step, QImage::Format_RGB888);
            ui->ref_red_vd->setPixmap(QPixmap::fromImage(display_r));

        }else{

            Calibrate.set_ref_by_camera(dual_1.current_frame(dual_1.bounding_rect),'R');
            int *ref_r = new int[3];
            ref_r = Calibrate.get_ref_rgb('R');

            Mat display_ref_r(ui->ref_red_vd->height(), ui->ref_red_vd->width(), CV_8UC3, Scalar(ref_r[0], ref_r[1], ref_r[2]));
            QImage display_r = QImage((uchar*)(display_ref_r.data), display_ref_r.cols, display_ref_r.rows, display_ref_r.step, QImage::Format_RGB888);
            ui->ref_red_vd->setPixmap(QPixmap::fromImage(display_r));
        }

        //set red start hs
        my_vd_it. rg_ratio_threshold_left = Calibrate.get_RG_Ratio_single_color(0, false);

        qDebug()<<"Initial Red RG:"<<my_vd_it. rg_ratio_threshold_left;

        if(calibrate_mode==1){
            AUO3D_imagePath_LR(PC_image_path(G),PC_image_path(G));
        }else if(calibrate_mode==0){
            Second_Monitor_mode(G_G);
        }

        vd_it_step->setMapping(calibrate_timer, 1);
        calibrate_timer->start();




    }

    else if(step==1){


        Mat tmp_dual_1 = dual_1.current_frame.clone();
        Mat tmp_dual_2 = dual_2.current_frame.clone();
      if(dual_1.side==1){
          //1號是右眼
        Calibrate.set_image_by_dual_camera(tmp_dual_2(dual_2.bounding_rect), tmp_dual_1(dual_1.bounding_rect));
      }else {
       Calibrate.set_image_by_dual_camera(tmp_dual_1(dual_1.bounding_rect), tmp_dual_2(dual_2.bounding_rect));
      }


    //set ref green and red threshold

        if(dual_1.side==1){
            Calibrate.set_ref_by_camera(dual_1.current_frame(dual_1.bounding_rect),'G');
            int *ref_g = new int[3];
            ref_g = Calibrate.get_ref_rgb('G');
            Mat display_ref_g(ui->ref_green_vd->height(), ui->ref_green_vd->width(), CV_8UC3, Scalar(ref_g[0], ref_g[1], ref_g[2]));
            QImage display_g = QImage((uchar*)(display_ref_g.data), display_ref_g.cols, display_ref_g.rows, display_ref_g.step, QImage::Format_RGB888);
            ui->ref_green_vd->setPixmap(QPixmap::fromImage(display_g));

        }else{
            Calibrate.set_ref_by_camera(dual_2.current_frame(dual_2.bounding_rect),'G');
            int *ref_g = new int[3];
            ref_g = Calibrate.get_ref_rgb('G');
            Mat display_ref_g(ui->ref_green_vd->height(), ui->ref_green_vd->width(), CV_8UC3, Scalar(ref_g[0], ref_g[1], ref_g[2]));
            QImage display_g = QImage((uchar*)(display_ref_g.data), display_ref_g.cols, display_ref_g.rows, display_ref_g.step, QImage::Format_RGB888);
            ui->ref_green_vd->setPixmap(QPixmap::fromImage(display_g));
        }

        my_vd_it.rg_ratio_threshold_right = Calibrate.get_RG_Ratio_single_color(1, false);
        my_vd_it.rg_ratio_threshold_left = (Calibrate.get_RG_Ratio_single_color(0, false) -  my_vd_it.rg_ratio_threshold_left) * RG_THRESHOLD ;
        qDebug()<<"Initial Green RG:"<<my_vd_it.rg_ratio_threshold_right;
        qDebug()<<"Red RG Threshold:"<< my_vd_it.rg_ratio_threshold_left;

        if(calibrate_mode==1){
            AUO3D_imagePath_LR(PC_image_path(R),PC_image_path(R));
        }else if(calibrate_mode==0){
            Second_Monitor_mode(R_R);
        }

       vd_it_step->setMapping(calibrate_timer,2);
       calibrate_timer->start();

    }
    else if(step==2){
        Mat tmp_dual_1 = dual_1.current_frame.clone();
        Mat tmp_dual_2 = dual_2.current_frame.clone();
      if(dual_1.side==1){
          //1號是右眼
        Calibrate.set_image_by_dual_camera(tmp_dual_2(dual_2.bounding_rect), tmp_dual_1(dual_1.bounding_rect));
      }else {
       Calibrate.set_image_by_dual_camera(tmp_dual_1(dual_1.bounding_rect), tmp_dual_2(dual_2.bounding_rect));
      }

         my_vd_it.rg_ratio_threshold_right = (Calibrate.get_RG_Ratio_single_color(1, false)- my_vd_it.rg_ratio_threshold_right) * RG_THRESHOLD  ;
         qDebug()<<"Green RG Threshold:"<<  my_vd_it.rg_ratio_threshold_right;

         if(calibrate_mode==1){
             AUO3D_imagePath_LR(PC_image_path(R),PC_image_path(G));
         }else if(calibrate_mode==0){
             Second_Monitor_mode(R_G);
         }
         ui->rg_right->setText(QString::number(my_vd_it.rg_ratio_threshold_right));
         ui->rg_left->setText(QString::number(my_vd_it.rg_ratio_threshold_left));

         calibrate_timer->setInterval(800);
         vd_it_step->setMapping(calibrate_timer,3);
         calibrate_timer->start();

    }
    else if(step==3){
        // find search case
        Mat tmp_dual_1 = dual_1.current_frame.clone();
        Mat tmp_dual_2 = dual_2.current_frame.clone();
      if(dual_1.side==1){
          //1號是右眼
        Calibrate.set_image_by_dual_camera(tmp_dual_2(dual_2.bounding_rect), tmp_dual_1(dual_1.bounding_rect));
      }else {
       Calibrate.set_image_by_dual_camera(tmp_dual_1(dual_1.bounding_rect), tmp_dual_2(dual_2.bounding_rect));
      }


         double rg_ratio_right = Calibrate.get_RG_Ratio_single_color_roi(1);
         double rg_ratio_left = Calibrate.get_RG_Ratio_single_color_roi(0);

        qDebug()<<"Green RG Threshold:"<<  my_vd_it.rg_ratio_threshold_right;
        qDebug()<<"Green RG :" << rg_ratio_right;
        qDebug()<<"Red RG Threshold:"<<  my_vd_it.rg_ratio_threshold_left;
        qDebug()<<"Red RG :" << rg_ratio_left;



        //ui->right->setText(QString::number(rg_ratio_right));
        //ui->left->setText(QString::number(rg_ratio_left));

        //if 沒有漏光 往兩側找->
        if (my_vd_it.rg_ratio_threshold_left > rg_ratio_left && my_vd_it.rg_ratio_threshold_right > rg_ratio_right) {
            my_vd_it.single_search = false;
            vd_it_step->setMapping(calibrate_timer,4);
            calibrate_timer->start();
            //to case 1

        }
        //出現漏光 往單方向找
        else{

            my_vd_it.single_search = true;

            if(calibrate_mode==0){
                if(my_vd_it.current_stage<3){
                      my_vd_it.tmp_C = parameters.C+my_vd_it.vd_adjust_step*10;
                    AUO3D_FPGA_SendData(PanelData::C,&my_vd_it.tmp_C);
                }
                else if(my_vd_it.current_stage==3){
                    my_vd_it.tmp_C = parameters.E1-my_vd_it.it_adjust_step*10;

                    AUO3D_FPGA_SendData(PanelData::E1,&my_vd_it.tmp_C);
                }else if(my_vd_it.current_stage==4){
                    my_vd_it.tmp_C =parameters.E1-my_vd_it.it_adjust_step*10;
                    AUO3D_FPGA_SendData(PanelData::E2,&my_vd_it.tmp_C);
                }

            }else if(calibrate_mode==1){
                if(my_vd_it.current_stage<3){
                      my_vd_it.tmp_C =  parameters.C+my_vd_it.vd_adjust_step*10;
                      AUO3D_SendData(PanelData::C,&my_vd_it.tmp_C);
                }
                else if(my_vd_it.current_stage==3){
                      my_vd_it.tmp_C = parameters.E1-my_vd_it.it_adjust_step*10;
                      AUO3D_SendData(PanelData::E1,&my_vd_it.tmp_C);
                }else if(my_vd_it.current_stage==4){
                      my_vd_it.tmp_C = parameters.E1-my_vd_it.it_adjust_step*10;
                      AUO3D_SendData(PanelData::E2,&my_vd_it.tmp_C);
                }
            }
            my_vd_it.rg_ratio_tmp_left = rg_ratio_left;
            my_vd_it.rg_ratio_tmp_right = rg_ratio_right;
            vd_it_step->setMapping(calibrate_timer,6);
            calibrate_timer->start();

            //to case 2

        }

    //雙向尋找
    }else if(step==4){
        //往正方向找邊界
        Mat tmp_dual_1 = dual_1.current_frame.clone();
        Mat tmp_dual_2 = dual_2.current_frame.clone();
      if(dual_1.side==1){
          //1號是右眼
        Calibrate.set_image_by_dual_camera(tmp_dual_2(dual_2.bounding_rect), tmp_dual_1(dual_1.bounding_rect));
      }else {
       Calibrate.set_image_by_dual_camera(tmp_dual_1(dual_1.bounding_rect), tmp_dual_2(dual_2.bounding_rect));
      }


        double rg_ratio_right = Calibrate.get_RG_Ratio_single_color_roi(1);
        double rg_ratio_left = Calibrate.get_RG_Ratio_single_color_roi(0);
        ui->right->setText(QString::number(rg_ratio_right));
        ui->left->setText(QString::number(rg_ratio_left));

        qDebug()<<"Green RG Threshold:"<<  my_vd_it.rg_ratio_threshold_right;
        qDebug()<<"Green RG :" << rg_ratio_right;
        qDebug()<<"Red RG Threshold:"<<  my_vd_it.rg_ratio_threshold_left;
        qDebug()<<"Red RG :" << rg_ratio_left;

        //在繼續尋找
        if (my_vd_it.rg_ratio_threshold_left >rg_ratio_left && my_vd_it.rg_ratio_threshold_right > rg_ratio_right) {
                    move_add();
                    vd_it_step->setMapping(calibrate_timer,4);
                    calibrate_timer->start();
        }else{
            //找另一個邊界
            move_dec();
            vd_it_step->setMapping(calibrate_timer,5);
            calibrate_timer->start();
        }

    }
    else if(step==5){
        //往負方向尋找
        Mat tmp_dual_1 = dual_1.current_frame.clone();
        Mat tmp_dual_2 = dual_2.current_frame.clone();
        if(dual_1.side==1){
            //1號是右眼
            Calibrate.set_image_by_dual_camera(tmp_dual_2(dual_2.bounding_rect), tmp_dual_1(dual_1.bounding_rect));
        }else {
            Calibrate.set_image_by_dual_camera(tmp_dual_1(dual_1.bounding_rect), tmp_dual_2(dual_2.bounding_rect));
        }


      double rg_ratio_right = Calibrate.get_RG_Ratio_single_color_roi(1);
      double rg_ratio_left = Calibrate.get_RG_Ratio_single_color_roi(0);
      ui->right->setText(QString::number(rg_ratio_right));
      ui->left->setText(QString::number(rg_ratio_left));

      qDebug()<<"Green RG Threshold:"<<  my_vd_it.rg_ratio_threshold_right;
      qDebug()<<"Green RG :" << rg_ratio_right;
      qDebug()<<"Red RG Threshold:"<<  my_vd_it.rg_ratio_threshold_left;
      qDebug()<<"Red RG :" << rg_ratio_left;

      //繼續尋找
      if (my_vd_it.rg_ratio_threshold_left >rg_ratio_left && my_vd_it.rg_ratio_threshold_right > rg_ratio_right) {
                  move_dec();
                  vd_it_step->setMapping(calibrate_timer,5);
                  calibrate_timer->start();
      }else{
       //找到邊界
          my_vd_it.add_move_length=0;
          my_vd_it.dec_move_length = 0;
          if( my_vd_it.current_stage<5)
            my_vd_it.current_stage++;
          dual_1.stop_update_roi=false;
          dual_2.stop_update_roi=false;
          vd_id_stage();

      }


    }

    //單方向尋找
    else  if(step==6){

        //find trend for single direction
        Mat tmp_dual_1 = dual_1.current_frame.clone();
        Mat tmp_dual_2 = dual_2.current_frame.clone();
        if(dual_1.side==1){
          //1號是右眼
          Calibrate.set_image_by_dual_camera(tmp_dual_2(dual_2.bounding_rect), tmp_dual_1(dual_1.bounding_rect));
        }else {
          Calibrate.set_image_by_dual_camera(tmp_dual_1(dual_1.bounding_rect), tmp_dual_2(dual_2.bounding_rect));
        }
      double rg_ratio_right = Calibrate.get_RG_Ratio_single_color_roi(1);
      double rg_ratio_left = Calibrate.get_RG_Ratio_single_color_roi(0);
      ui->right->setText(QString::number(rg_ratio_right));
      ui->left->setText(QString::number(rg_ratio_left));


       if( my_vd_it.rg_ratio_tmp_left >  rg_ratio_left || my_vd_it.rg_ratio_tmp_right >  rg_ratio_right){
           //有改善
          qDebug()<<"better--------------";

           //set back
           if(my_vd_it.current_stage<3){
                   my_vd_it.single_search_is_add = true;
                 my_vd_it.tmp_C = parameters.C;
               if(calibrate_mode==0){
                    AUO3D_FPGA_SendData(PanelData::C,&my_vd_it.tmp_C);
               }else if(calibrate_mode==1){
                    AUO3D_SendData(PanelData::C,&my_vd_it.tmp_C);
               }
           }else if(my_vd_it.current_stage==3){
                my_vd_it.single_search_is_add = false;
               my_vd_it.tmp_C = parameters.E1;
               if(calibrate_mode==0){
                    AUO3D_FPGA_SendData(PanelData::E1,&my_vd_it.tmp_C);
               }else if(calibrate_mode==1){
                    AUO3D_SendData(PanelData::E1,&my_vd_it.tmp_C);
               }

           }else if(my_vd_it.current_stage==4){
                 my_vd_it.single_search_is_add = false;
               my_vd_it.tmp_C = parameters.E2;
               if(calibrate_mode==0){
                    AUO3D_FPGA_SendData(PanelData::E2,&my_vd_it.tmp_C);
               }else if(calibrate_mode==1){
                    AUO3D_SendData(PanelData::E2,&my_vd_it.tmp_C);
               }


           }

           vd_it_step->setMapping(calibrate_timer,7);
           calibrate_timer->start();

       }else {
           qDebug()<<"worse--------------";


           if(my_vd_it.current_stage<3){
                 my_vd_it.single_search_is_add =true;
               my_vd_it.tmp_C = parameters.C;
               if(calibrate_mode==0){
                    AUO3D_FPGA_SendData(PanelData::C,&my_vd_it.tmp_C);
               }else if(calibrate_mode==1){
                    AUO3D_SendData(PanelData::C,&my_vd_it.tmp_C);
               }
           }else if(my_vd_it.current_stage==3){
              my_vd_it.single_search_is_add =true;
               my_vd_it.tmp_C = parameters.E1;
               if(calibrate_mode==0){
                    AUO3D_FPGA_SendData(PanelData::E1,&my_vd_it.tmp_C);
               }else if(calibrate_mode==1){
                    AUO3D_SendData(PanelData::E1,&my_vd_it.tmp_C);
               }

           }else if(my_vd_it.current_stage==4){
                 my_vd_it.single_search_is_add =true;
               my_vd_it.tmp_C = parameters.E2;
               if(calibrate_mode==0){
                    AUO3D_FPGA_SendData(PanelData::E2,&my_vd_it.tmp_C);
               }else if(calibrate_mode==1){
                    AUO3D_SendData(PanelData::E2,&my_vd_it.tmp_C);
               }

           }
            vd_it_step->setMapping(calibrate_timer,7);
            calibrate_timer->start();

       }




    }
    else if(step==7){

      Mat tmp_dual_1 = dual_1.current_frame.clone();
      Mat tmp_dual_2 = dual_2.current_frame.clone();
      if(dual_1.side==1){
            //1號是右眼
            Calibrate.set_image_by_dual_camera(tmp_dual_2(dual_2.bounding_rect), tmp_dual_1(dual_1.bounding_rect));
      }else {
            Calibrate.set_image_by_dual_camera(tmp_dual_1(dual_1.bounding_rect), tmp_dual_2(dual_2.bounding_rect));
      }
      double rg_ratio_right = Calibrate.get_RG_Ratio_single_color_roi(1);
      double rg_ratio_left = Calibrate.get_RG_Ratio_single_color_roi(0);
      ui->right->setText(QString::number(rg_ratio_right));
      ui->left->setText(QString::number(rg_ratio_left));


      qDebug()<<"it add"<< my_vd_it.add_move_length;
      qDebug()<<"it dec"<< my_vd_it.dec_move_length;


      //如果RG 不夠好
       if (my_vd_it.rg_ratio_threshold_left < rg_ratio_left || my_vd_it.rg_ratio_threshold_right < rg_ratio_right){


           //找到二次邊界
           if(my_vd_it.add_move_length!=0 && my_vd_it.dec_move_length!=0)
           {

               qDebug()<<"it no good2 --------------";
               qDebug()<<"no finish--------------";
               //退回邊界
               (my_vd_it.single_search_is_add) ? move_dec() : move_add();
               //校正結束
                my_vd_it.single_search_is_add =false;
                my_vd_it.single_search=false;
                my_vd_it.add_move_length = 0;
                my_vd_it.dec_move_length = 0;
                if( my_vd_it.current_stage<5){
                  my_vd_it.current_stage++;
                 }
                dual_1.stop_update_roi=false;
                dual_2.stop_update_roi=false;
                vd_id_stage();
                return;
           }

           if(my_vd_it.single_search_is_add){
                 qDebug()<<"it no good 1";
                move_add();

           }else{
                 qDebug()<<"it no good 1";
                move_dec();
           }
           //(my_vd_it.single_search_is_add) ? move_add() : move_dec();

           vd_it_step->setMapping(calibrate_timer,7);
           calibrate_timer->start();
       }

    else{
        //找到一次邊界
        //往政方向找

        if(my_vd_it.single_search_is_add){
            qDebug()<<"it good--------------";
            //如果第一次邊界剛找到
            if(my_vd_it.current_stage<3){
                if(my_vd_it.dec_move_length==0){
                     my_vd_it.dec_move_length = my_vd_it.add_move_length;
                }
            }
            else{
                if(my_vd_it.dec_move_length==0){
                     my_vd_it.dec_move_length = my_vd_it.add_move_length;
                }
            }
            //尋找二次邊界
            move_add();
            vd_it_step->setMapping(calibrate_timer,7);
            calibrate_timer->start();
        }
        //往復方向
        else{
            qDebug()<<"it good--------------";
             //如果第一次邊界剛找到
            if(my_vd_it.current_stage<3){
                if(my_vd_it.add_move_length==0){
                     my_vd_it.add_move_length = my_vd_it.dec_move_length;
                }
            }
            else{
                if(my_vd_it.add_move_length==0){
                     my_vd_it.add_move_length = my_vd_it.dec_move_length;
                }
            }
            move_dec();
            vd_it_step->setMapping(calibrate_timer,7);
            calibrate_timer->start();
        }
    }

 }



    //wide angle cross talk check
    else if(step==8){

        //decide left and right
        Mat dual_1_roi = dual_1.current_frame(dual_1.bounding_rect).clone();
        Mat dual_2_roi = dual_2.current_frame(dual_2.bounding_rect).clone();
        int side_left = Calibrate.find_right_and_left_camera(dual_1_roi,dual_2_roi);

        if(side_left==1){
            dual_1.side = 1;
            dual_2.side = 2;
        }else {
            dual_2.side = 1;
            dual_1.side = 2;
        }

        if(calibrate_mode==1){
            AUO3D_imagePath_LR(PC_image_path(W),PC_image_path(B));
        }else if(calibrate_mode==0){
            Second_Monitor_mode(W_B);
        }
        vd_it_step->setMapping(calibrate_timer,9);
        calibrate_timer->start();
    }


    else if(step==9){
        //get W_B IMage
        if(dual_1.side==1){
            //1 is right
             tmp_right_black = dual_1.current_frame(dual_1.bounding_rect).clone();
             tmp_left_white = dual_2.current_frame(dual_2.bounding_rect).clone();
        }else{
            tmp_right_black = dual_2.current_frame(dual_2.bounding_rect).clone();
            tmp_left_white = dual_1.current_frame(dual_1.bounding_rect).clone();
        }
        if(calibrate_mode==1){
            AUO3D_imagePath_LR(PC_image_path(B),PC_image_path(W));
        }else if(calibrate_mode==0){
            Second_Monitor_mode(B_W);
        }
        vd_it_step->setMapping(calibrate_timer, 10);
        calibrate_timer->start();


    }

    else if(step==10){
        //get B_W IMage
        if(dual_1.side==1){
            //1 is right
            tmp_right_white = dual_1.current_frame(dual_1.bounding_rect).clone();
            tmp_left_black = dual_2.current_frame(dual_2.bounding_rect).clone();
        }else{
            tmp_right_white = dual_2.current_frame(dual_2.bounding_rect).clone();
            tmp_left_black = dual_1.current_frame(dual_1.bounding_rect).clone();
        }

        if(calibrate_mode==1){
            AUO3D_imagePath_LR(PC_image_path(W),PC_image_path(B));
        }else if(calibrate_mode==0){
            Second_Monitor_mode(W_B);
        }

        if(my_vd_it.search_dir_add==true){
            vd_it_step->setMapping(calibrate_timer, 11);
            calibrate_timer->start();
        }else{
            vd_it_step->setMapping(calibrate_timer, 12);
            calibrate_timer->start();
        }


    }

    else if(step==11){
        //calulate cross talk and shift x
        double right_crosstalk_mean;
        double left_crosstalk_mean;


        double ratio_step=0.1;
        Mat right_crosstalk_map = Calibrate.get_crosstalk_map(tmp_right_white,tmp_right_black,&right_crosstalk_mean);
        Mat left_crosstalk_map = Calibrate.get_crosstalk_map(tmp_left_white,tmp_left_black,&left_crosstalk_mean);

        if(right_crosstalk_map.empty() || left_crosstalk_map.empty()){
            qDebug()<<"error";
            return;
        }

        //Record image
        Mat right_crosstalk_img = Calibrate.draw_crosstalk(tmp_right_black,right_crosstalk_map,ratio_step);
        Mat left_crosstalk_img = Calibrate.draw_crosstalk(tmp_left_black,left_crosstalk_map,ratio_step);
        if(right_crosstalk_img.empty() || left_crosstalk_img.empty()){
            qDebug()<<"error";
            return;
        }
        Mat result_r;
        Mat result_l;
        Mat view;
        cv::vconcat(right_crosstalk_img,tmp_right_black, result_r );
        cv::vconcat(left_crosstalk_img,tmp_left_black, result_l );
        cv::resize(result_l,result_l, result_r.size());
        cv::hconcat( result_l, result_r, view );
        //Record End

        int right = Calibrate.find_view_edge(right_crosstalk_map,0.1,0.05,1.0,true);
        int left = Calibrate.find_view_edge(left_crosstalk_map,0.1,0.05,1.0,false);

        if(right ==1 || left ==1){

            if(!find_positive_edge){
                //first time find edge
                 find_positive_edge = true;
                 positive_view_length = my_vd_it.x_add_length - my_vd_it.x_adjust_step;
             }

         }

        //qDebug()<<tmp_crosstalk_right;
        tmp_crosstalk_right = Calibrate.current_cross_talk_right;
        tmp_crosstalk_left = Calibrate.current_cross_talk_left;
        positive.push_back({view,tmp_left_black,tmp_left_white,tmp_right_black,tmp_right_white,tmp_crosstalk_right,tmp_crosstalk_left});

        if(my_vd_it.x_add_length!=20){
            //in Center 100% area as total pixels , IF number of (crosstalk ratio > 10%  pixels ) > 5% of total pixels => find edge
            // 1 is not find edge yet
            //find positive side
            my_vd_it.x_add_length = my_vd_it.x_add_length + my_vd_it.x_adjust_step;

            my_vd_it.tmp_x = my_vd_it.set_x + my_vd_it.x_add_length;
            qDebug()<<"123::::"<< my_vd_it.tmp_x;
            if(calibrate_mode==0){
                 AUO3D_FPGA_SendData(PanelData::X, &my_vd_it.tmp_x);
            }else if(calibrate_mode==1){
                 AUO3D_SendData(PanelData::X, &my_vd_it.tmp_x);
            }

            vd_it_step->setMapping(calibrate_timer, 9);
            calibrate_timer->start();
        }

        else if(my_vd_it.x_add_length == 20){
            // 2 is find edge
            //go to negtive side
            my_vd_it.search_dir_add = false;

            my_vd_it.view_zone_mm =  my_vd_it.x_add_length - my_vd_it.x_adjust_step;
            my_vd_it.x_add_length = 0;
            my_vd_it. x_dec_length = my_vd_it.x_adjust_step;

            my_vd_it.tmp_x = my_vd_it.set_x -  my_vd_it.x_adjust_step;
            if(calibrate_mode==0){
                 AUO3D_FPGA_SendData(PanelData::X, &my_vd_it.tmp_x);
            }else if(calibrate_mode==1){
                 AUO3D_SendData(PanelData::X, &my_vd_it.tmp_x);
            }

           vd_it_step->setMapping(calibrate_timer,9);
            calibrate_timer->start();
        }

    }


    else if(step==12){
        //adjust x dec

        double right_crosstalk_mean;
        double left_crosstalk_mean;
        double ratio_step=0.1;
        Mat right_crosstalk_map = Calibrate.get_crosstalk_map(tmp_right_white,tmp_right_black,&right_crosstalk_mean);
        Mat left_crosstalk_map = Calibrate.get_crosstalk_map(tmp_left_white,tmp_left_black,&right_crosstalk_mean);

            if(right_crosstalk_map.empty() || left_crosstalk_map.empty()){
                qDebug()<<"error";
                return;
            }

            //Record image
            Mat right_crosstalk_img = Calibrate.draw_crosstalk(tmp_right_black,right_crosstalk_map,ratio_step);
            Mat left_crosstalk_img = Calibrate.draw_crosstalk(tmp_left_black,left_crosstalk_map,ratio_step);
            if(right_crosstalk_img.empty() || left_crosstalk_img.empty()){
                qDebug()<<"error";
                return;
            }
            Mat result_r;
            Mat result_l;
            Mat view;
            cv::vconcat(right_crosstalk_img,tmp_right_black, result_r );
            cv::vconcat(left_crosstalk_img,tmp_left_black, result_l );
            cv::resize(result_l,result_l, result_r.size());
            cv::hconcat(result_l,result_r, view );

            image_num++;

            // Record End

            int right = Calibrate.find_view_edge(right_crosstalk_map,0.1,0.05,1.0,true);
            int left = Calibrate.find_view_edge(left_crosstalk_map,0.1,0.05,1.0,false);


           if(right ==1 || left ==1){
               if(!find_negative_edge){
                   find_negative_edge = true;
                   negative_view_length = my_vd_it.x_dec_length - my_vd_it.x_adjust_step;
               }

            }

           tmp_crosstalk_right = Calibrate.current_cross_talk_right;
           tmp_crosstalk_left = Calibrate.current_cross_talk_left;

           negative.push_back({view,tmp_left_black,tmp_left_white,tmp_right_black,tmp_right_white,tmp_crosstalk_right,tmp_crosstalk_left });


            if(my_vd_it.x_dec_length!=20){
                //find negtive side
                my_vd_it.x_dec_length =  my_vd_it.x_dec_length + my_vd_it.x_adjust_step;
                my_vd_it.tmp_x = my_vd_it.set_x - my_vd_it.x_dec_length;
                if(calibrate_mode==0){
                     AUO3D_FPGA_SendData(PanelData::X, &my_vd_it.tmp_x);
                }else if(calibrate_mode==1){
                     AUO3D_SendData(PanelData::X, &my_vd_it.tmp_x);
                }
                vd_it_step->setMapping(calibrate_timer, 9);
                calibrate_timer->start();
            }
            else if(my_vd_it.x_dec_length==20){

                std::reverse(negative.begin(), negative.end());
                negative.insert(negative.end(),positive.begin(),positive.end());

                QString filePath = "./crosstalk _wide_view/view_zone.txt" ;
                QFile file(filePath);

             if (file.open(QIODevice::WriteOnly | QIODevice::Text)){
                 QTextStream stream(&file);
                 stream<<"\n"<<"[View Zone Data]\n";
                 stream <<"View Zone in mm :"<< QString::number( positive_view_length + negative_view_length )<<"\n";
                 stream << "View Zone in positive side length(mm) :"<< QString::number(positive_view_length)<<"\n";
                 //stream << "image number in positive edge  :"<< QString::number(positive_view_length/0.1+1)<<"\n";
                 stream << "View Zone in negative side length(mm) :"<< QString::number(negative_view_length)<<"\n";
                 //stream << "image number in negative edge  :"<< QString::number(negative_view_length/0.1)<<"\n";

                 for(int i=0 ;i<negative.size();i++ ){
                     imwrite("./crosstalk _wide_view/right_origin/"+QString::number(i).toStdString()+".jpg" ,negative.at(i).rb);
                     //imwrite("./crosstalk/left_origin/"+QString::number(i).toStdString()+".jpg" ,negative.at(i).rw);
                     imwrite("./crosstalk _wide_view/left_origin/"+QString::number(i).toStdString()+".jpg" ,negative.at(i).lb);
                     //imwrite("./crosstalk/lw_"+QString::number(i).toStdString()+".jpg" ,negative.at(i).lw);
                     imwrite("./crosstalk _wide_view/full_view/"+QString::number(i).toStdString()+".jpg" ,negative.at(i).full_image);
                     stream << QString::number(i)+"_right:"+ QString::number(negative.at(i).cross_talk_right)<<"\n";
                     stream << QString::number(i)+"_left:"+ QString::number(negative.at(i).cross_talk_left)<<"\n";
                 }
                 file.close();
             }


                my_vd_it.search_dir_add = true;

                negative.clear();
                positive.clear();

                //end Calibration
                ui->start_calibrate_vd->setText(QString::fromLocal8Bit("校正完成"));
                ui->to_vd_it->setEnabled(true);
                ui->Calibration_Function_Box->setEnabled(true);
                ui->start_calibrate_vd->setEnabled(true);
                ui->calibrate_next->show();
                disconnect(calibrate_timer, SIGNAL(timeout()),  vd_it_step, SLOT(map()));

                my_vd_it.x_add_length = 0;
                my_vd_it.x_dec_length = 0;
                if(calibrate_mode==1){

                    AUO3D_imagePath_LR("./RED.bmp","./GREEN.bmp");
                }else if(calibrate_mode==0){

                    Second_Monitor_mode(R_G);
                }

            }



    }
}

void MainWindow::move_add(){

    //VD CASE
    if(my_vd_it.current_stage<3){
             my_vd_it.add_move_length =  my_vd_it.add_move_length+my_vd_it.vd_adjust_step;
             my_vd_it.tmp_C = my_vd_it.C+ my_vd_it.add_move_length;
             set_par();
             if(calibrate_mode==0){
                  AUO3D_FPGA_SendData(PanelData::C,&my_vd_it.tmp_C);
             }else if(calibrate_mode==1){
                  AUO3D_SendData(PanelData::C,&my_vd_it.tmp_C);
             }
    }else if(my_vd_it.current_stage==3){
              my_vd_it.add_move_length =  my_vd_it.add_move_length + +my_vd_it.it_adjust_step;
              my_vd_it.tmp_C =  parameters.E1 + my_vd_it.add_move_length;
              set_par();
              if(calibrate_mode==0){
                  AUO3D_FPGA_SendData(PanelData::E1,&my_vd_it.tmp_C);
              }else if(calibrate_mode==1){
                  AUO3D_SendData(PanelData::E1,&my_vd_it.tmp_C);
              }
    }else if(my_vd_it.current_stage==4){
              my_vd_it.add_move_length =  my_vd_it.add_move_length + my_vd_it.it_adjust_step;
              my_vd_it.tmp_C = parameters.E2 + my_vd_it.add_move_length;
              set_par();
              if(calibrate_mode==0){
                  AUO3D_FPGA_SendData(PanelData::E2,&my_vd_it.tmp_C);
              }else if(calibrate_mode==1){
                  AUO3D_SendData(PanelData::E2,&my_vd_it.tmp_C);
              }

    }

}

void MainWindow::move_dec(){
    //VD CASE
    if(my_vd_it.current_stage<3){
             my_vd_it.dec_move_length =  my_vd_it.dec_move_length+my_vd_it.vd_adjust_step;
             my_vd_it.tmp_C =  my_vd_it.C + -1*my_vd_it.dec_move_length;
             set_par();
             if(calibrate_mode==0){
                  AUO3D_FPGA_SendData(PanelData::C,&my_vd_it.tmp_C);
             }else if(calibrate_mode==1){
                  AUO3D_SendData(PanelData::C,&my_vd_it.tmp_C);
             }
    }else if(my_vd_it.current_stage==3){
             my_vd_it.dec_move_length =  my_vd_it.dec_move_length +my_vd_it.it_adjust_step;
             my_vd_it.tmp_C =  parameters.E1 + -1*my_vd_it.dec_move_length;
             set_par();
             if(calibrate_mode==0){
                  AUO3D_FPGA_SendData(PanelData::E1,&my_vd_it.tmp_C);
             }else if(calibrate_mode==1){
                  AUO3D_SendData(PanelData::E1,&my_vd_it.tmp_C);
             }
    }else if(my_vd_it.current_stage==4){
             my_vd_it.dec_move_length =  my_vd_it.dec_move_length +my_vd_it.it_adjust_step;
             my_vd_it.tmp_C =   parameters.E2 +-1* my_vd_it.dec_move_length;
             set_par();
             if(calibrate_mode==0){
                  AUO3D_FPGA_SendData(PanelData::E2,&my_vd_it.tmp_C);
             }else if(calibrate_mode==1){
                  AUO3D_SendData(PanelData::E2,&my_vd_it.tmp_C);
             }

    }


}

void MainWindow::set_par(){
    //IT worse: add true
    if(my_vd_it.current_stage==0){

        if (!my_vd_it.single_search) {
                    my_vd_it.C1 = (my_vd_it.add_move_length + -1 * my_vd_it.dec_move_length) / 2;
                    ui->C1->setText(QString::number(my_vd_it.C1));

                }
                else {
                    my_vd_it.C1 = (my_vd_it.single_search_is_add) ? ((my_vd_it.add_move_length+my_vd_it.dec_move_length) / 2) : (-1 * (my_vd_it.dec_move_length + my_vd_it.add_move_length) / 2);
                     ui->C1->setText(QString::number(my_vd_it.C1));
        }
    }
    else if(my_vd_it.current_stage==1){
        if (!my_vd_it.single_search) {
                    my_vd_it.C2 = (my_vd_it.add_move_length + -1 * my_vd_it.dec_move_length) / 2;
                     ui->C2->setText(QString::number(my_vd_it.C2));
                }
                else {
                    my_vd_it.C2 = (my_vd_it.single_search_is_add) ? ((my_vd_it.add_move_length+my_vd_it.dec_move_length) / 2) : (-1 * (my_vd_it.dec_move_length + my_vd_it.add_move_length) / 2);
                     ui->C2->setText(QString::number(my_vd_it.C2));
        }

    }else if(my_vd_it.current_stage==2){

        if (!my_vd_it.single_search) {
                    my_vd_it.C3 = (my_vd_it.add_move_length + -1 * my_vd_it.dec_move_length) / 2;
                    ui->C3->setText(QString::number(my_vd_it.C3));
        }

         else {
             my_vd_it.C3 = (my_vd_it.single_search_is_add) ? ((my_vd_it.add_move_length+my_vd_it.dec_move_length) / 2) : (-1 * (my_vd_it.dec_move_length + my_vd_it.add_move_length) / 2);
              ui->C3->setText(QString::number(my_vd_it.C3));

        }

    }else if(my_vd_it.current_stage==3){

        if (!my_vd_it.single_search) {
              my_vd_it.E1 = parameters.E1+(my_vd_it.add_move_length + -1*my_vd_it.dec_move_length) / 2;
              ui->E1->setText(QString::number(my_vd_it.E1));
        }

        else {
              my_vd_it.E1 = (my_vd_it.single_search_is_add) ?  parameters.E1+((my_vd_it.add_move_length+my_vd_it.dec_move_length) / 2) : parameters.E1-((my_vd_it.dec_move_length + my_vd_it.add_move_length) / 2);
              ui->E1->setText(QString::number(my_vd_it.E1));
        }

    }else if(my_vd_it.current_stage==4){
        if (!my_vd_it.single_search) {
              my_vd_it.E2 = parameters.E2+(my_vd_it.add_move_length + -1*my_vd_it.dec_move_length) / 2;
              ui->E2->setText(QString::number(my_vd_it.E2));
        }

        else {
              my_vd_it.E2 = (my_vd_it.single_search_is_add) ? parameters.E2+((my_vd_it.add_move_length+my_vd_it.dec_move_length) / 2) : parameters.E2-( (my_vd_it.dec_move_length + my_vd_it.add_move_length) / 2);
              ui->E2->setText(QString::number(my_vd_it.E2));

        }

    }

}

void MainWindow::vd_id_stage(){
    if(my_vd_it.current_stage==0){
        double VD = parameters.OVD;
        if(calibrate_mode==0){
              AUO3D_FPGA_SendData(PanelData::VD,&VD);
        }else if(calibrate_mode==1){
              AUO3D_SendData(PanelData::VD,&VD);
        }
        my_vd_it.current_x = 0;
        my_vd_it.current_vd = VD;
         my_vd_it.current_y = 0;

        ui->eye_pos_x->setText(QString::number(-5)+"~"+QString::number(5));
        ui->eye_pos_y->setText(QString::number(-10)+"~"+QString::number(10));
        ui->eye_pos_z->setText(QString::number( VD-5 )+"~"+QString::number( VD+5 ));

        ui->start_calibrate_vd->setText(QString::fromLocal8Bit("開始校正"));
        ui->start_calibrate_vd->setEnabled(true);


    }
    else if(my_vd_it.current_stage==1){
        //C1 to C2
        double VD = (parameters.OVD+parameters.vd_far)/2;
        if(calibrate_mode==0){
             AUO3D_FPGA_SendData(PanelData::VD,&VD);
        }else if(calibrate_mode==1){
             AUO3D_SendData(PanelData::VD,&VD);
             // AUO3D_SendData(PanelData::C,&my_vd_it.C1);
        }

        my_vd_it.current_x = 0;
        my_vd_it.current_vd = VD;
         my_vd_it.current_y = 0;
        ui->eye_pos_x->setText(QString::number(-5)+"~"+QString::number(5));
        ui->eye_pos_y->setText(QString::number(-10)+"~"+QString::number(10));
        ui->eye_pos_z->setText(QString::number( VD-5 )+"~"+QString::number( VD+5 ));


        //QPixmap camera_position = load_image("./2.png",ui->vd_pos->width(),ui->vd_pos->height());
        //ui->vd_pos->setPixmap(camera_position);
        ui->start_calibrate_vd->setText(QString::fromLocal8Bit("開始校正C2"));
        ui->start_calibrate_vd->setEnabled(true);

    }
    else if(my_vd_it.current_stage==2){

        //C2 to C3
        double VD = parameters.vd_far;
        if(calibrate_mode==0){
             AUO3D_FPGA_SendData(PanelData::VD,&VD);
        }else if(calibrate_mode==1){
             AUO3D_SendData(PanelData::VD,&VD);
        }

        my_vd_it.current_x = 0;
        my_vd_it.current_vd = VD;
         my_vd_it.current_y = 0;
        ui->eye_pos_x->setText(QString::number(-5)+"~"+QString::number(5));
        ui->eye_pos_y->setText(QString::number(-10)+"~"+QString::number(10));
        ui->eye_pos_z->setText(QString::number( VD-5 )+"~"+QString::number( VD+5 ));


        //QPixmap camera_position = load_image("./3.png",ui->vd_pos->width(),ui->vd_pos->height());
        //ui->vd_pos->setPixmap(camera_position);
        ui->start_calibrate_vd->setText(QString::fromLocal8Bit("開始校正C3"));
        ui->start_calibrate_vd->setEnabled(true);

    }
    else if(my_vd_it.current_stage==3){

        //C3 to E1
        double X = 0.36396471 * (parameters.OVD+parameters.vd_far)/2;
        double VD = (parameters.OVD+parameters.vd_far)/2;

        //Get A B C
        //
        double a1 = parameters.OVD*parameters.OVD;
        double b1 = parameters.OVD;
        double c1 = 1;
        double a2 = ((parameters.OVD+parameters.vd_far)/2) *  ((parameters.OVD+parameters.vd_far)/2);
        double b2  = (parameters.OVD+parameters.vd_far)/2;
        double c2 = 1;
        double a3 = parameters.vd_far*parameters.vd_far;
        double b3 = parameters.vd_far;
        double c3 = 1;
        double det = a1*(b2*c3-b3*c2)- b1*(a2*c3-a3*c2)+ c1*(a2*b3-a3*b2);
        double d1 = my_vd_it.C1;
        double d2 = my_vd_it.C2;
        double d3 = my_vd_it.C3;

        my_vd_it.A =  (d1 * (b2 * c3 - b3 * c2) - b1 * (d2 * c3 - d3 * c2) + c1 * (d2 * b3 - d3 * b2)) / det;
        my_vd_it.B =   (a1 * (d2 * c3 - d3 * c2) - d1 * (a2 * c3 - a3 * c2) + c1 * (a2 * d3 - a3 * d2)) / det;
        my_vd_it.C =  (a1 * (b2 * d3 - b3 * d2) - b1 * (a2 * d3 - a3 * d2) + d1 * (a2 * b3 - a3 * b2)) / det;

        ui->A->setText(QString::number(my_vd_it.A));
        ui->B->setText(QString::number(my_vd_it.B));
        ui->C->setText(QString::number(my_vd_it.C));



        if(calibrate_mode==0){
            AUO3D_FPGA_SendData(PanelData::X,&X);
             AUO3D_FPGA_SendData(PanelData::VD,&VD);
             AUO3D_FPGA_SendData(PanelData::A,&my_vd_it.A);
             AUO3D_FPGA_SendData(PanelData::B,&my_vd_it.B);
              AUO3D_FPGA_SendData(PanelData::C,&my_vd_it.C);

        }else if(calibrate_mode==1){
            AUO3D_SendData(PanelData::X,&X);
             AUO3D_SendData(PanelData::VD,&VD);
            AUO3D_SendData(PanelData::A,&my_vd_it.A);
            AUO3D_SendData(PanelData::B,&my_vd_it.B);
            AUO3D_SendData(PanelData::C,&my_vd_it.C);
        }

        my_vd_it.current_x = X;
        my_vd_it.current_vd = VD;
         my_vd_it.current_y = 0;
        ui->eye_pos_x->setText(QString::number(X-5)+"~"+QString::number(X+5));
        ui->eye_pos_y->setText(QString::number(-10)+"~"+QString::number(10));
        ui->eye_pos_z->setText(QString::number( VD-5 )+"~"+QString::number( VD+5 ));


        //QPixmap camera_position = load_image("./44.png",ui->vd_pos->width(),ui->vd_pos->height());
        //ui->vd_pos->setPixmap(camera_position);
        ui->start_calibrate_vd->setText(QString::fromLocal8Bit("開始校正E1"));
        ui->start_calibrate_vd->setEnabled(true);
    }
    else if(my_vd_it.current_stage==4){
        //E1 to E2
        double X =  -1*TAN_20 * (parameters.OVD+parameters.vd_far)/2;
        double VD = (parameters.OVD+parameters.vd_far)/2;

        if(calibrate_mode==0){
             AUO3D_FPGA_SendData(PanelData::E1,&my_vd_it.E1);
            AUO3D_FPGA_SendData(PanelData::X,&X);
             AUO3D_FPGA_SendData(PanelData::VD,&VD);
        }else if(calibrate_mode==1){
             AUO3D_SendData(PanelData::E1,&my_vd_it.E1);
            AUO3D_SendData(PanelData::X,&X);
             AUO3D_SendData(PanelData::VD,&VD);
        }

        ui->eye_pos_x->setText(QString::number(X-5)+"~"+QString::number(X+5));
        ui->eye_pos_y->setText(QString::number(-5)+"~"+QString::number(5));
        ui->eye_pos_z->setText(QString::number( VD-5 )+"~"+QString::number( VD+5 ));


        //QPixmap camera_position = load_image("./5.png",ui->vd_pos->width(),ui->vd_pos->height());
        //ui->vd_pos->setPixmap(camera_position);
        ui->start_calibrate_vd->setText(QString::fromLocal8Bit("開始校正E2"));
        ui->start_calibrate_vd->setEnabled(true);

    }else if(my_vd_it.current_stage==5){

        //finish
        double X =  -1*TAN_20 * (parameters.OVD+parameters.vd_far)/2;
        double VD = (parameters.OVD+parameters.vd_far)/2;

        my_vd_it.current_x = X;
        my_vd_it.current_vd = VD;
         my_vd_it.current_y = 0;

        if(calibrate_mode==0){
             AUO3D_FPGA_SendData(PanelData::X,&X);
             AUO3D_FPGA_SendData(PanelData::VD,&VD);
             AUO3D_FPGA_SendData(PanelData::E1,&my_vd_it.E1);
             AUO3D_FPGA_SendData(PanelData::E2,&my_vd_it.E2);


        }else if(calibrate_mode==1){
             AUO3D_SendData(PanelData::X,&X);
             AUO3D_SendData(PanelData::VD,&VD);
             AUO3D_SendData(PanelData::E1,&my_vd_it.E1);
             AUO3D_SendData(PanelData::E2,&my_vd_it.E2);

        }
        ui->eye_pos_x->setText(QString::number(X-5)+"~"+QString::number(X+5));
        ui->eye_pos_y->setText(QString::number(-10)+"~"+QString::number(10));
        ui->eye_pos_z->setText(QString::number( VD-5 )+"~"+QString::number( VD+5 ));


        //QPixmap camera_position = load_image("./5.png",ui->vd_pos->width(),ui->vd_pos->height());
        //ui->vd_pos->setPixmap(camera_position);
        ui->start_calibrate_vd->setText(QString::fromLocal8Bit("校正完成"));
        ui->to_vd_it->setStyleSheet("QPushButton{background-color: rgb(46, 125, 161);}");
        ui->to_vd_it->setEnabled(true);
        ui->Calibration_Function_Box->setEnabled(true);
        ui->start_calibrate_vd->setEnabled(true);
        ui->calibrate_next->show();
        disconnect(calibrate_timer, SIGNAL(timeout()),  vd_it_step, SLOT(map()));

        my_calibrate.a = my_vd_it.A;
        my_calibrate.b = my_vd_it.B;
        my_calibrate.c = my_vd_it.C;
        my_calibrate.e1 = my_vd_it.E1;
        my_calibrate.e2 = my_vd_it.E2;

        //restart the calibration
        my_vd_it.current_stage=0;
    }

}

void MainWindow::eye_tracking_ui(){
    float x=0;
    float y=0;
    float vd=0;
    if(calibrate_mode==0){
        x = fpga_eye_tracking.x+my_calibrate.cam_off_x;
        y=fpga_eye_tracking.y+my_calibrate.cam_off_y;
        vd = fpga_eye_tracking.z+my_calibrate.cam_off_vd;


    }else if(calibrate_mode==1){
        x = pc_eye_tracking.x+my_calibrate.cam_off_x;
        y= pc_eye_tracking.y+my_calibrate.cam_off_y;
        vd = pc_eye_tracking.z+my_calibrate.cam_off_vd;
    }

    x_filter.push_back(x);
    vd_filter.push_back(vd);

    if(x_filter.size()==10){
            double tmp_x_mean=0.0;
            double tmp_vd_mean=0.0;
            for(int i=0; i<10 ; i++){
                tmp_x_mean += x_filter.at(i);
                tmp_vd_mean += vd_filter.at(i);
            }
            tmp_x_mean = std::floor(tmp_x_mean * 100) / 1000;
            tmp_vd_mean = std::floor(tmp_vd_mean * 100) / 1000;
            x_filter.erase(x_filter.begin());
            vd_filter.erase(vd_filter.begin());

            if(current_page==7){
                ui->eye_x->setText(QString::number(tmp_x_mean));
                ui->eye_y->setText(QString::number(y));
                ui->eye_z->setText(QString::number(tmp_vd_mean));

            }else if(current_page==8){

                ui->eye_x_2->setText(QString::number(tmp_x_mean));
                ui->eye_y_2->setText(QString::number(y));
                ui->eye_z_2->setText(QString::number(tmp_vd_mean));

            }
    }
}
//-----------------------------------------------------Page 9 Function(Finish)

void MainWindow::save_file(){
    QString folderPath = QFileDialog::getExistingDirectory(this, tr("選擇儲存的資料夾"), "", QFileDialog::ShowDirsOnly);

    /*
    QString filePath = QFileDialog::getSaveFileName(nullptr, "Save File", "", "Text Files (123.ini)");
    if (filePath.isEmpty()) {
         return;
    }
    */

    if (!folderPath.isEmpty()) {

        QString fileName = QString::fromStdString(parameters.module_name)+"_"+QString::fromStdString(parameters.panel_id)+".ini"; // 替換成您想要的檔案名稱
        QString filePath = folderPath + "/" + fileName;


         QFile file(filePath);

         if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
             QTextStream stream(&file);

             stream<<"\n"<<"[Camera Position & View Zone]\n";
             stream<<"panel_id="<<QString::fromStdString(parameters.panel_id)<<"\n";
             stream<<"Module_Type="<<QString::fromStdString(parameters.module_name)<<"\n";
             stream<<"CAM_OFT_X="<<QString::number(my_calibrate.cam_off_x)<<"\n";
             stream<<"CAM_OFT_Y="<<QString::number(my_calibrate.cam_off_y)<<"\n";
             stream<<"CAM_OFT_Z="<<QString::number(my_calibrate.cam_off_vd)<<"\n";
             stream<<"FOV_H_min="<<QString::number(parameters.fov_h_min)<<"\n";
             stream<<"FOV_H_max="<<QString::number(parameters.fov_h_max)<<"\n";
             stream<<"FOV_V_min="<<QString::number(parameters.fov_v_min)<<"\n";
             stream<<"FOV_V_max="<<QString::number(parameters.fov_v_max)<<"\n";
             stream<<"FOV_VD_min="<<QString::number(parameters.fov_vd_min)<<"\n";
             stream<<"FOV_VD_max="<<QString::number(parameters.fov_vd_max)<<"\n";
             stream<<"resolutoin_x="<<QString::number(parameters.resolutoin_x)<<"\n";
             stream<<"resolutoin_y="<<QString::number(parameters.resolution_y)<<"\n";
             stream<<"\n"<<"[Slant HCP]\n";
             stream<<"OVD="<<QString::number(parameters.OVD)<<"\n";
             stream<<"VD_FAR="<<QString::number(parameters.vd_far)<<"\n";
             stream<<"Slant="<<QString::number(my_calibrate.slant)<<"\n";
             stream<<"HCP="<<QString::number(my_calibrate.hcp)<<"\n";
             stream<<"Xoff_Lens="<<QString::number(my_calibrate.xoff_lens)<<"\n";
             stream<<"\n"<<"[WS_OVD]\n";
             stream<<"WS_OVD="<<QString::number(my_calibrate.ws_ovd)<<"\n";
             stream<<"\n"<<"[Xoff_VD]\n";
             stream<<"a="<<QString::number(my_calibrate.a)<<"\n";
             stream<<"b="<<QString::number(my_calibrate.b)<<"\n";
             stream<<"c="<<QString::number(my_calibrate.c)<<"\n";
             stream<<"\n"<<"[IT]\n";
             stream<<"A1=0\n B1=0\nC1=0\nD1=0\n";
             stream<<"E1="<<QString::number(my_calibrate.e1)<<"\n";
             stream<<"A2=0\n B2=0\nC2=0\nD2=0\n";
             stream<<"E2="<<QString::number(my_calibrate.e2)<<"\n";
             stream<<"\n"<<"[Calibration Step]\n";
             stream<<"hcp_adjust_value="<<QString::number(my_hcp_slant.hcp_adjust_value)<<"\n";
             stream<<"slant_adjust_value="<<QString::number(my_hcp_slant.slant_adjust_value)<<"\n";
             stream<<"Xoff_adjust_step="<<QString::number(my_xoff_lens.Xoff_adjust_value)<<"\n";
             stream<<"WS_OVD_adjust_step="<<QString::number(my_ws_ovd.ws_ovd_adjust_step)<<"\n";
             stream<<"View_Zone_adjust_step="<<QString::number(my_view_zone.view_adjust_step)<<"\n";
             stream<<"VD_adjust_step="<<QString::number(my_vd_it.vd_adjust_step)<<"\n";
             stream<<"IT_adjust_step="<<QString::number(my_vd_it.it_adjust_step)<<"\n";

             stream<<"\n"<<"[EMMC]\n";
             QDate currentDate = QDate::currentDate();
             stream<<"Version=1.0\n";
             stream<<"Update date="<<currentDate.toString()<<"\n";
             stream<<"\n"<<"[Effect Setting]\n";
             stream<<"Enable_3D=1Enable_Gamma=0\nEnable_FOV=0\nLR_Switch=1\nInterpolationDP=1\nCalibration=0\n";

             file.close();
             ui->file_warn->setText(QString::fromLocal8Bit("儲存成功"));
             ui->file_warn->setStyleSheet("QLabel { color : white; }");

         } else {
             // 如果无法打开文件，输出错误信息
             qDebug() << "Error: Unable to open the file.";
         }
    }else{
        return;
    }



}

void MainWindow::load_to_emmc(){

    uint8_t command = 1;
    uint8_t status = -1;
    bool check_write=false;
    int res = AUO3D_FPGA_SendData(PanelData::EMMC_WRITE, &command);
    if(res==0){
        qDebug()<<"fpga connect fail: when write emmc";
        return;
    }else{
         qDebug()<<"fpga connect Success";

         clock_t call_start, call_time;
         call_start = clock();
         call_time = clock();


         while((double(call_time) - double(call_start)) <5000){
             if(AUO3D_FPGA_GetData(PanelData::STATUS_EMMC_Write, &status)>0){
                 if(status==0 || status==1){
                     check_write= true;
                     break;
                 }
             }
             call_time = clock();
         }
         if(check_write){
             ui->fpga_warn->setText(QString::fromLocal8Bit("寫入成功"));
             ui->fpga_warn->setStyleSheet("QLabel { color : white; }");
             ui->fpga_status->setStyleSheet("QLabel { color : white; }");
             ui->fpga_status->setText(QString::fromLocal8Bit("FPGA 校正模組:: 已連線"));
             eye_tracking_thread_start();
             Sleep(1000);
             check_FPGA_status();
         }else{
             ui->fpga_warn->setText(QString::fromLocal8Bit("寫入失敗"));
             ui->fpga_warn->setStyleSheet("QLabel { color : white; }");
             ui->fpga_status->setStyleSheet("QLabel { color : white; }");
             ui->fpga_status->setText(QString::fromLocal8Bit("FPGA 校正模組:: 已連線"));
             eye_tracking_thread_start();
             Sleep(1000);
             check_FPGA_status();
         }


    }


}

void MainWindow::back_to_calibration(){

    if(calibrate_mode==0){
        uint8_t* tmp_uns_data = (uint8_t*)malloc(sizeof(uint8_t) * 1);
        *tmp_uns_data = 1;
        int res = AUO3D_FPGA_SendData(PanelData::CALIBRATION, tmp_uns_data);
    }else if(calibrate_mode==1) {
        uint8_t* tmp_uns_data = (uint8_t*)malloc(sizeof(uint8_t) * 1);
        *tmp_uns_data = 1;
        int res = AUO3D_SendData(PanelData::CALIBRATION, tmp_uns_data);
    }
    Sleep(300);
    ui->Calibration_Function_Box->show();
    to_vd_it_page();
    ui->calibrate_next->show();

}

void MainWindow::final_check(){

    if(ui->human_check_box->isChecked() && ui->camera_check_box->isChecked()){
        ui->save_result->setEnabled(true);
        ui->to_emmc->setEnabled(true);
    }else{
        ui->save_result->setEnabled(false);
        ui->to_emmc->setEnabled(false);
    }

}

void MainWindow::check_content_switch_RG(){

        if(calibrate_mode==0){
            Second_Monitor_mode(R_G);

        }else if(calibrate_mode==1){

            AUO3D_imagePath_LR("./RED.bmp","./GREEN.bmp");
        }
        ui->camera_check->setEnabled(false);
         ui->human_check->setEnabled(true);



}

void MainWindow::check_content_switch_Detail(){

        if(calibrate_mode==0){
            Second_Monitor_mode(Check);

        }else if(calibrate_mode==1){

            AUO3D_imagePath_LR("./L.bmp","./R.bmp");
        }

        ui->camera_check->setEnabled(true);
        ui->human_check->setEnabled(false);

}
