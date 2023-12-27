#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QDialog>
#include <QMessageBox>
#include <QSignalMapper>
#include <QTimer>
#include<QDesktopWidget>
#include<QWindow>
#include <QDebug>
#include <QScreen>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QFile>
#include <QTextStream>
#include <QDate>
#include <QMainWindow>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include "AUO3DLib/include/AUO3DLib.h"
#include "FPGA/AUO3DLib_FPGA.h"
#include "Calibrate_Process.h"
#include "DeviceEnumerator.h"


QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

    Mat tmp_right_black;
    Mat tmp_right_white;
    Mat tmp_left_black;
    Mat tmp_left_white;
    float xoff_count=0.0;


    int count_slant = 0;
    int count_hcp = 0;
    double RG_THRESHOLD = 0.2;
    int CALIBRATE_UPDATE_TIME = 1500;

    D3Calibrate_lib Calibrate; //calibrate lib
    enum Pannel_Pattern {
        R,
        G,
        B,
        W,
        R_R,
        G_G,
        R_G,
        G_R,
        W_B,
        B_W,
        C_C,
        C,
        Check
    };
    const string pattern_name[13] = {
        "./RED.bmp",
        "./GREEN.bmp",
        "./BLACK.bmp",
        "./WHITE.bmp",
        "./RED.bmp",
        "./GREEN.bmp",
        "./RED_GREEN.bmp",
        "./GREEN_RED.bmp",
        "./WHITE_BLACK.bmp",
        "./BLACK_WHITE.bmp",
        "./Center_Center.bmp",
        "./Center.png",
        "./06.bmp"
    };

    const char* PC_image_path(int pattern){

        return pattern_name[pattern].c_str();
    }

    struct camera_attribute
    {
        int id=-1;
        bool is_close=true;
        cv::Mat current_frame;
        int side = -1; //0: left 1:right
        bool is_contour=false;
        bool stop_update_roi = false;
        bool is_roi_open = false;
        std::vector<std::vector<cv::Point>> coutours;
        std::vector<cv::Vec4i> hierarchy;
        Rect  bounding_rect;
    };
    camera_attribute single,dual_1,dual_2;

    struct calibrate_par{
        double hcp=0;
        double slant=0;
        double xoff_lens=0;
        double ws_ovd=0;
        double a=0;
        double b=0;
        double c=0;
        double e1=0;
        double e2=0;
        double view_zone=0;
          double view_zone_mm = 0;

        double cam_off_x=0;
        double cam_off_y=0;
        double cam_off_vd=0;

    };
    calibrate_par my_calibrate;




    void open_dual_camera_1(){
        std::thread t1(&D3Calibrate_lib::open_camera, Calibrate, dual_1.id, &dual_1.is_close, &dual_1.current_frame,true);
        t1.detach();
    }
    void open_dual_camera_2(){
        std::thread t2(&D3Calibrate_lib::open_camera, Calibrate, dual_2.id, &dual_2.is_close, &dual_2.current_frame,true);
        t2.detach();
    }
    void open_single_camera(){
        std::thread t3(&D3Calibrate_lib::open_camera, Calibrate, single.id, &single.is_close, &single.current_frame,false);
        t3.detach();
    }

    void check_camera_stream_thread(){
        std::thread t1(&MainWindow::check_camera_stream,this);
        t1.detach();
    }

    void check_camera_stream();

    void eye_tracking_update(float *x,float *y, float*z, int *refesh_rate, bool *is_get_eye_tracking) {
        int res = 0;
        float eye_position[3];
        do {
            res = AUO3D_FPGA_GetData(PanelData::EYE_POSITION, eye_position);
            if (res) {
                *x = eye_position[0];
                *y = eye_position[1];
                *z = eye_position[2];
            }
            else {
                *x = -1;
                *y = -1;
                *z = -1;
                *is_get_eye_tracking = false;
                break;
            }
            Sleep(*refesh_rate);
        } while (*is_get_eye_tracking);

    }
    void eye_tracking_update_pc(float *x,float *y, float*z, int *refesh_rate, bool *is_get_eye_tracking) {
        int res = 1;
        float *eye_position_x = new float[1];
        float *eye_position_y = new float[1];
        float *eye_position_z = new float[1];

        do {
            AUO3D_getETK(eye_position_x,eye_position_y,eye_position_z);
            if (res) {
                *x = eye_position_x[0];
                *y = eye_position_y[0];
                *z = eye_position_z[0];
            }
            else {
                *x = -1;
                *y = -1;
                *z = -1;
                *is_get_eye_tracking = false;
                break;
            }
            //Sleep(*refesh_rate);
        } while (*is_get_eye_tracking);

    }

private:
    bool fpga_connect=false;
    int current_page = 0; //0
    int last_page = 0;
    int calibrate_mode = -1; // 0:FPGA base 1:PC base
    void vd_id_stage();
    //PANEL PAR
    Panel_parameters parameters;
    //HCP_SLANT_PAR
    struct hcp_slant_data{
        double best_color_contrast=0.0;
        double best_color_hcp=0.0;
        int color_search_time=0;
        double ref_red_h;
        double ref_red_s;
        double ref_green_h;
        double ref_green_s;
        double hcp;
        double slant;
        double ws_ovd;
        double hcp_adjust_value = 0.01;
        double hcp_adjust_step = hcp_adjust_value / 2;
        double hcp_adjust_range = hcp_adjust_value * 0.1;
        double color_distance_adjust_step = hcp_adjust_range * 0.5;
        double max_hcp = 999;
        double min_hcp = -1;
        double slant_adjust_value = 0.1;
        double slant_adjust_step = slant_adjust_value / 2;
        double slant_adjust_range = slant_adjust_value * 0.1;
        double max_slant = 999;
        double min_slant = -1;
        double best_rg = 999;
        double best_hcp = 0.0;
        double best_slant = 0.0;
        double last_rg_ratio=999.9;
        int check_times=0;
        int trend=1;
        bool is_trend;
        bool is_calibrating=false;
    };
    hcp_slant_data my_hcp_slant;
    //XOFF_PAR
    struct xoff_lesn_data{
        double Xoff_len;
        bool adjust_finish = false;
        int adjust_direction =0;
        double first_pixel_distance=0.0;
        double Xoff_adjust_step=0.1;
        double Xoff_adjust_value = 0.0;
        bool is_calibrating=false;
    };
    xoff_lesn_data my_xoff_lens;
    //WS_OVD_PAR
    struct ws_ovd_data{
        double ws_ovd=0.0;
        bool is_calibrating=false;
        double best_rg_ratio=9999;
        int    rg_ratio_check_times = 0;
        double ws_ovd_adjust_step = 1;
        Mat tmp_frame;
    };
    ws_ovd_data my_ws_ovd;
    //VIEW_ZONE_PAR
    struct view_zone_data{
        double view_zone;
        int panel_level;
        double ref_r_1,ref_r_2;
        double ref_g_1,ref_g_2;

        double rg_ratio_threshold_left;
        double rg_ratio_threshold_right;

        double view_adjust_step =0.05;
        double right_view_zone_length = 0;
        double left_view_zone_length = 0;

        double tmp_xoff_len;


       bool is_calibrating=false;

    };
    view_zone_data my_view_zone;
    //VD_IT_PAR
    struct vd_it_data{
        double A;
        double B;
        double C;
        double tmp_C;
        double E1;
        double E2;
        double C1=0;
        double C2=0;
        double C3=0;

        bool single_search=false;
        bool single_search_is_add = false;

        double rg_ratio_tmp_left;
        double rg_ratio_tmp_right;

        double rg_ratio_threshold_left;
        double rg_ratio_threshold_right;

        double add_move_length=0;
        double dec_move_length=0;
        double vd_adjust_step= 0.05;
        double it_adjust_step = 0.01;

        int current_stage = 0;

        //0 C1
        //1 C2
        //2 C3
        //3 E1
        //4 E2
        bool is_calibrating=false;

        float current_x=-1;
        float current_vd=-1;
        float current_y=-1;


    };
    vd_it_data my_vd_it;

    //EYE_TRACKING_PAR
    struct FPGA_eye_tracking_data {
            float eye_position[3];
            float x = -1;
            float y = -1;
            float z = -1;
            int refresh_time = 60;
            bool is_get_eye_tracking = false;
        };
    FPGA_eye_tracking_data fpga_eye_tracking;

    struct PC_eye_tracking_data {
            float eye_position[3];
            float x = -1;
            float y = -1;
            float z = -1;
            int refresh_time = 60;
            bool is_get_eye_tracking = false;
        };
    PC_eye_tracking_data pc_eye_tracking;

    //QT UI PAR
    Ui::MainWindow *ui;
    struct pattern_layout {
        QWidget* widget;
        QPixmap img;
        QLabel* label;
    }; pattern_layout second_monitor;

    void MainWindow::Second_Monitor_mode(int mode);
    QSize resolution;
    QPixmap load_image(QString path,int width,int height);
    QTimer *camera_fresh_timer;
    QTimer *Eye_Tracking_timer;
    QTimer *calibrate_timer;
    QSignalMapper* hcp_slant_step;
    QSignalMapper* xoff_lens_step;
    QSignalMapper* ws_ovd_step;
    QSignalMapper* view_zone_step;
    QSignalMapper* vd_it_step;
    QSignalMapper* pass_check_map;
    void MainWindow::closeEvent(QCloseEvent* event);

    //FUNCTION
    void initial();
    void check_PC_status(){
        std::thread t1(&MainWindow::pc_is_running,this);
        t1.detach();
    }
    void connect_fpga_(){
        std::thread t1(&MainWindow::fpga_ip_connect,this);
        t1.detach();
    }
    void check_FPGA_status(){
        std::thread t1(&MainWindow::fpga_is_running,this);
        t1.detach();
    }
    void eye_tracking_thread_start() {
            fpga_eye_tracking.is_get_eye_tracking = true;
            std::thread t1(&MainWindow::eye_tracking_update ,this, &fpga_eye_tracking.x, &fpga_eye_tracking.y, &fpga_eye_tracking.z, &fpga_eye_tracking.refresh_time, &fpga_eye_tracking.is_get_eye_tracking);
            t1.detach();
     }

    void pc_eye_tracking_thread_start() {
            pc_eye_tracking.is_get_eye_tracking = true;
            std::thread t1(&MainWindow::eye_tracking_update_pc ,this, &pc_eye_tracking.x, &pc_eye_tracking.y, &pc_eye_tracking.z, &pc_eye_tracking.refresh_time, &pc_eye_tracking.is_get_eye_tracking);
            t1.detach();
     }

    void open_roi_thread_1(){
        int camera_num=1;
        std::thread t1(&MainWindow::start_roi_detection,this, camera_num);
        t1.detach();
    }
    void open_roi_thread_2(){
         int camera_num =2;
        std::thread t1(&MainWindow::start_roi_detection,this,camera_num);
        t1.detach();
    }

    void pc_is_running();
    void fpga_is_running();
    bool fpga_status=false;
    bool pc_status=false;
    void start_roi_detection(int camera_num);
    void reset_parameters();


private slots:
    void animationStackedWidgets();
    void whenAnimationFinish();
    void page_add();
    void page_dec();
    void to_next_calibrate_step();
    void to_env_setting_page();
    void to_camera_setting_page();
    void to_hcp_slant_page();
    void to_xoff_len_page();
    void to_ws_ovd_page();
    void to_view_zone_test_page();
    void to_vd_it_page();
    void to_finish_page();
    void calibrate_terminate();

    //page 0 function
    void select_FPGA();
    void select_PC();

    //page 1 function
    void fpga_ip_connect();
    void start_calibrate();
    int load_parameters();
    void load_ini_file();

    //page 2 function
    void reconnect_camera();
    void check_camera_device();
    void preview_camera();

    //page 3 function
    void start_hcp();
    void calibrate_hcp(int step);

    //page 4 function
    void start_xoff_lens();
    void calibrate_xoff_lens(int step);

    //page 5 function
    void start_ws_ovd();
    void calibrate_ws_ovd(int step);

    void ws_test();

    //page 6 function
    void start_view_zone();
    void calibrate_view_zone(int step);

    //page 7 function
    void start_vd();
    void calibrate_vd(int step);

    void move_add();
    void move_dec();
    void set_par();

    void eye_tracking_ui();
    //page 8 function
    void save_file();
    void load_to_emmc();
    void back_to_calibration();
    void restart();

    //page 9 function

    void final_check();
    void pass_check(int res);
    void check_content_switch_RG();
    void check_content_switch_Detail();

 signals:
    void camera_loss();
};
#endif // MAINWINDOW_H
