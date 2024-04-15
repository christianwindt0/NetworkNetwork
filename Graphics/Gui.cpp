/**
 * @file      Gui.cpp
 * @author    Christian Windt (cwindt0@gmail.com)
 * @license   MIT
 * @copyright Copyright (c) 2023  Shenzhen Xin Yuan Electronic Technology Co., Ltd
 * @date      2023-07-13
 *
 */

#include <LilyGo_AMOLED.h>
#include "gui.h"
#include <Adafruit_NeoPixel.h>
#include <WiFi.h>
#include "LocalDNS.h"


extern LilyGo_Class amoled;

LV_FONT_DECLARE(alibaba_font_48);
LV_FONT_DECLARE(alibaba_font_18);

LV_FONT_DECLARE(font_ali_70);

LV_IMG_DECLARE(nwLogo);
LV_IMG_DECLARE(icon_battery);
LV_IMG_DECLARE(icon_cpu);
LV_IMG_DECLARE(icon_flash);
LV_IMG_DECLARE(icon_ram);
LV_IMG_DECLARE(icon_light_sensor);
LV_IMG_DECLARE(icon_usb);
LV_IMG_DECLARE(icon_micro_sd);

static lv_obj_t *tileview;
extern Adafruit_NeoPixel pixels;
// Save the ID of the current page
static uint8_t pageId = 0;


void DNS_UI(lv_obj_t *parent) {

    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(cont, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);

    lv_obj_t *title = lv_label_create(cont);
    lv_label_set_text_fmt(title, "Network Weasel DNS");
    lv_obj_set_style_text_color(title, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, -75);

    lv_obj_t *btns_bg = lv_obj_create(cont);
    lv_obj_set_width(btns_bg, lv_pct(90));
    lv_obj_set_style_bg_color(btns_bg, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_border_width(btns_bg, 0, 0);
    lv_obj_align_to(btns_bg, title, LV_ALIGN_OUT_BOTTOM_MID, 23, 40);

    lv_obj_set_flex_flow(btns_bg, LV_FLEX_FLOW_ROW);

    lv_obj_t *btn1 = lv_btn_create(btns_bg);
    lv_obj_set_size(btn1, lv_pct(45), 90); // 18pct
    lv_obj_t *label1 = lv_label_create(btn1);
    lv_label_set_text(label1, "Start DNS");
    lv_obj_center(label1);



    lv_obj_t *btn2 = lv_btn_create(btns_bg);
    lv_obj_set_size(btn2, lv_pct(45), 90); // 18pct
    lv_obj_t *label2 = lv_label_create(btn2);
    lv_label_set_text(label2, "Stop DNS");
    lv_obj_center(label2);



    lv_obj_t *side_arrow = lv_label_create(cont);
    lv_label_set_text_fmt(side_arrow, "<");
    lv_obj_set_style_text_color(side_arrow, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_text_align(side_arrow, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(side_arrow, LV_ALIGN_RIGHT_MID, -5, 0);

    // Status Label
    lv_obj_t *status_label = lv_label_create(cont);
    lv_label_set_text(status_label, "Status: Idle");
    lv_obj_set_style_text_color(status_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(status_label, LV_ALIGN_CENTER, 0, -45);

    // Store status label in user data for access in callbacks
    lv_obj_set_user_data(btn1, status_label);
    lv_obj_set_user_data(btn2, status_label);


    lv_obj_add_event_cb(btn1, [](lv_event_t *e) {
        lv_obj_t *bg = (lv_obj_t *)lv_obj_get_user_data(lv_event_get_target(e));        
        lv_label_set_text(bg, "Status: Running");

        // Code to start DNS server here
        if (startDnsServer()){
            lv_label_set_text(bg, "Status: Running");
        }
        else{
            lv_label_set_text(bg, "Status: Failed");
        }
    }, LV_EVENT_CLICKED, NULL);

    lv_obj_add_event_cb(btn2, [](lv_event_t *e) {
        lv_obj_t *bg = (lv_obj_t *)lv_obj_get_user_data(lv_event_get_target(e));        
        lv_label_set_text(bg, "Status: Stopped");

        // Code to stop DNS server here
        stopDnsServer();
    }, LV_EVENT_CLICKED, NULL);
}


// Animation complete callback
static void animation_complete_cb(lv_anim_t *anim) {
    lv_obj_t *logo_img = (lv_obj_t *)anim->var;
    lv_obj_del(logo_img);  // Delete the image after fading
}


void createStartupScreen(lv_obj_t *parent) {
    // Create a full-screen black object
    lv_obj_t *black_bg = lv_obj_create(parent);
    lv_obj_set_size(black_bg, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(black_bg, lv_color_black(), LV_PART_MAIN);

    // Ensure no scroll bars
    lv_obj_clear_flag(black_bg, LV_OBJ_FLAG_SCROLLABLE);

    // Create an image object to hold the logo
    lv_obj_t *logo_img = lv_img_create(black_bg);
    lv_img_set_src(logo_img, &nwLogo);
    lv_img_set_antialias(logo_img, true);

    lv_coord_t img_width_original = nwLogo.header.w; // Original width of the image
    lv_coord_t img_height_original = nwLogo.header.h; // Original height of the image

    lv_coord_t parent_w = lv_obj_get_width(parent);
    lv_coord_t parent_h = lv_obj_get_height(parent);

    int scale_factor = (parent_w / 2 * 256) / img_width_original; 

    // Set the zoom level based on the calculated scale factor
    lv_img_set_zoom(logo_img, scale_factor);

    // Center the image on the black background
    lv_obj_center(logo_img);


    // Create fade animation
    lv_anim_t a;
    lv_anim_t b;

    lv_anim_init(&a);
    lv_anim_init(&b);

    lv_anim_set_var(&a, logo_img); 
    lv_anim_set_var(&b, black_bg);

    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
    lv_anim_set_exec_cb(&b, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);

    lv_anim_set_values(&a, LV_OPA_COVER, LV_OPA_TRANSP);
    lv_anim_set_values(&b, LV_OPA_COVER, LV_OPA_TRANSP);

    lv_anim_set_time(&a, 750); // Duration of fade effect
    lv_anim_set_time(&b, 750); // Duration of fade effect

    lv_anim_set_delay(&a, 1750); // Delay before starting the fade
    lv_anim_set_delay(&b, 1750); // Delay before starting the fade

    lv_anim_set_ready_cb(&a, animation_complete_cb);
    lv_anim_set_ready_cb(&b, animation_complete_cb); 

    lv_anim_start(&a);
    lv_anim_start(&b);



}


static void slider_event_cb(lv_event_t *e)
{
    lv_obj_t *slider = lv_event_get_target(e);
    lv_obj_t *slider_label =  (lv_obj_t *)lv_event_get_user_data(e);
    uint8_t level = (uint8_t)lv_slider_get_value(slider);
    int percentage = map(level, 100, 255, 0, 100);
    lv_label_set_text_fmt(slider_label, "%u%%", percentage);
    lv_obj_align_to(slider_label, slider, LV_ALIGN_CENTER, 0, 0);
    amoled.setBrightness(level);
}

void createBrightnessUI(lv_obj_t *parent)
{
    lv_obj_t *label;
    uint8_t b = amoled.getBrightness();
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_bg_color(cont, lv_color_black(), 0);
    lv_obj_set_size(cont, lv_disp_get_physical_hor_res(NULL), lv_disp_get_ver_res(NULL) );
    lv_obj_remove_style(cont, 0, LV_PART_SCROLLBAR);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_center(cont);
    lv_obj_set_style_border_width(cont, 0, 0);

    lv_obj_t *left_arrow = lv_label_create(parent);
    lv_label_set_text_fmt(left_arrow, ">");
    lv_obj_set_style_text_color(left_arrow, lv_color_make(255, 255, 255), LV_PART_MAIN);
    lv_obj_set_style_text_align(left_arrow, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(left_arrow, LV_ALIGN_BOTTOM_LEFT, 5, -5);

    lv_obj_t *right_arrow = lv_label_create(parent);
    lv_label_set_text_fmt(right_arrow, "<");
    lv_obj_set_style_text_color(right_arrow, lv_color_make(255, 255, 255), LV_PART_MAIN);
    lv_obj_set_style_text_align(right_arrow, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(right_arrow, LV_ALIGN_BOTTOM_RIGHT, -5, -5);

    // MAC
    label = lv_label_create(cont);
    lv_label_set_text_fmt(label, "MAC:%s", WiFi.macAddress().c_str());
    lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);

    // IPADDRESS
    label = lv_label_create(cont);
    lv_label_set_text(label, "IP:NONE" );
    lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
    lv_msg_subsribe_obj(WIFI_MSG_ID, label, NULL);

    // Added got ip address message cb
    lv_obj_add_event_cb( label, [](lv_event_t *e) {
        lv_obj_t *label = (lv_obj_t *)lv_event_get_target(e);
        Serial.print("-->>IP:"); Serial.println(WiFi.localIP());
        lv_label_set_text_fmt(label, "IP:%s",  WiFi.isConnected() ? (WiFi.localIP().toString().c_str()) : ("NONE") );
    }, LV_EVENT_MSG_RECEIVED, NULL);


    // Temperature
    label = lv_label_create(cont);
    lv_label_set_text(label, "Temperature:0°C");
    lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
    lv_msg_subsribe_obj(TEMPERATURE_MSG_ID, label, NULL);


    //SDCard
    const  BoardsConfigure_t *boards = amoled.getBoarsdConfigure();
    if (boards->sd) {
        label = lv_label_create(cont);
        if (SD.cardType() != CARD_NONE) {
            lv_label_set_text_fmt(label, "SDCard: %u MBytes", (uint32_t)(SD.cardSize() / 1024 / 1024));
        } else {
            lv_label_set_text(label, "SDCard: NULL");
        }
        lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
        lv_msg_subsribe_obj(TEMPERATURE_MSG_ID, label, NULL);
    }

    // BRIGHTNESS
    label = lv_label_create(cont);
    lv_label_set_text(label, "Brightness:");
    lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
    lv_msg_subsribe_obj(WIFI_MSG_ID, label, NULL);

    /*Create a slider and add the style*/
    lv_obj_t *slider = lv_slider_create(cont);
    lv_obj_set_size(slider, lv_disp_get_physical_hor_res(NULL) - 60, 30);
    lv_slider_set_range(slider, 100, 255);
    lv_slider_set_value(slider, b, LV_ANIM_OFF);

    /*Create a label below the slider*/
    lv_obj_t *slider_label = lv_label_create(slider);
    lv_label_set_text_fmt(slider_label, "%lu%%", map(b, 0, 255, 0, 100));
    lv_obj_set_style_text_color(slider_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, slider_label);
    lv_obj_align_to(slider_label, slider, LV_ALIGN_CENTER, 0, 0);
}


static void pixels_event_handler(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);
    uint8_t *index = (uint8_t *)lv_obj_get_user_data(target);
    if (!index)return;
    if (code == LV_EVENT_CLICKED || code == LV_EVENT_VALUE_CHANGED ) {
        switch (*index) {
        case 0:
            pixels.setPixelColor(0, pixels.Color(255, 0, 0)); pixels.show();
            break;
        case 1:
            pixels.setPixelColor(0, pixels.Color(0, 255, 0)); pixels.show();
            break;
        case 2:
            pixels.setPixelColor(0, pixels.Color(0, 0, 255)); pixels.show();
            break;
        case 3: {
            lv_obj_t *cw =  (lv_obj_t *)lv_event_get_user_data(e);
            lv_color_t c =  lv_colorwheel_get_rgb(cw);
            pixels.setPixelColor(0, pixels.Color(c.ch.red, (c.ch.green_h << 3) | c.ch.green_l, c.ch.blue)); pixels.show();
        }
        break;
        case 4: {
            lv_obj_t *slider_label =  (lv_obj_t *)lv_event_get_user_data(e);
            uint8_t level = (uint8_t)lv_slider_get_value(target);
            int percentage = map(level, 0, 255, 0, 100);
            lv_label_set_text_fmt(slider_label, "%u%%", percentage);
            lv_obj_align_to(slider_label, target, LV_ALIGN_CENTER, 0, 0);
            uint8_t val = (uint8_t)lv_slider_get_value(target);
            pixels.setBrightness(val);
            pixels.show();
        }
        break;
        default:
            break;
        }
    }
}

void createDeviceInfoUI(lv_obj_t *parent)
{
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_bg_color(cont, lv_color_black(), 0);
    lv_obj_set_size(cont, lv_disp_get_physical_hor_res(NULL), lv_disp_get_ver_res(NULL) );
    lv_obj_remove_style(cont, 0, LV_PART_SCROLLBAR);
    lv_obj_center(cont);
    lv_obj_set_style_border_width(cont, 0, 0);

    lv_obj_t *left_arrow = lv_label_create(parent);
    lv_label_set_text_fmt(left_arrow, ">");
    lv_obj_set_style_text_color(left_arrow, lv_color_make(255, 255, 255), LV_PART_MAIN);
    lv_obj_set_style_text_align(left_arrow, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(left_arrow, LV_ALIGN_BOTTOM_LEFT, 5, -5);

    lv_obj_t *right_arrow = lv_label_create(parent);
    lv_label_set_text_fmt(right_arrow, "<");
    lv_obj_set_style_text_color(right_arrow, lv_color_make(255, 255, 255), LV_PART_MAIN);
    lv_obj_set_style_text_align(right_arrow, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(right_arrow, LV_ALIGN_BOTTOM_RIGHT, -5, -5);

    static lv_style_t font_style;
    lv_style_init(&font_style);
    lv_style_set_text_color(&font_style, lv_color_white());
    lv_style_set_text_font(&font_style, &lv_font_montserrat_18);

    // CPU
    lv_obj_t *img_cpu = lv_img_create(cont);
    lv_img_set_src(img_cpu, &icon_cpu);
    lv_obj_align(img_cpu, LV_ALIGN_LEFT_MID, 55, -50);

    lv_obj_t *label = lv_label_create(cont);
    lv_obj_add_style(label, &font_style, 0);
    lv_label_set_text(label, ESP.getChipModel());
    lv_obj_align_to(label, img_cpu, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);


    //Flash
    lv_obj_t *img_flash = lv_img_create(cont);
    lv_img_set_src(img_flash, &icon_flash);
    lv_obj_align_to(img_flash, img_cpu, LV_ALIGN_OUT_RIGHT_MID, 40, 0);

    label = lv_label_create(cont);
    lv_obj_add_style(label, &font_style, 0);
    lv_label_set_text_fmt(label, "%uMB", ESP.getFlashChipSize() / 1024 / 1024);
    lv_obj_align_to(label, img_flash, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);


    //PSRAM
    lv_obj_t *img_ram = lv_img_create(cont);
    lv_img_set_src(img_ram, &icon_ram);
    lv_obj_align_to(img_ram, img_flash, LV_ALIGN_OUT_RIGHT_MID, 40, 0);

    float ram_size = abs(ESP.getPsramSize() / 1024.0 / 1024.0);
    label = lv_label_create(cont);
    lv_obj_add_style(label, &font_style, 0);
    lv_label_set_text_fmt(label, "%.1fMB", ram_size);
    lv_obj_align_to(label, img_ram, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);

    static lv_obj_t *pdat[3];

    //BATTERY
    lv_obj_t *img_battery = lv_img_create(cont);
    lv_img_set_src(img_battery, &icon_battery);
    lv_obj_align_to(img_battery, img_cpu, LV_ALIGN_OUT_BOTTOM_MID, 0, 35);

    label = lv_label_create(cont);
    lv_obj_add_style(label, &font_style, 0);
    lv_label_set_text(label, "N/A");
    lv_obj_align_to(label, img_battery, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
    pdat[0] = label;

    //USB
    lv_obj_t *img_usb = lv_img_create(cont);
    lv_img_set_src(img_usb, &icon_usb);
    lv_obj_align_to(img_usb, img_flash, LV_ALIGN_OUT_BOTTOM_MID, 0, 35);

    label = lv_label_create(cont);
    lv_obj_add_style(label, &font_style, 0);
    lv_label_set_text(label, "N/A");
    lv_obj_align_to(label, img_usb, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
    pdat[1] = label;

    //SENSOR
    lv_obj_t *img_sensor = lv_img_create(cont);
    lv_img_set_src(img_sensor, &icon_light_sensor);
    lv_obj_align_to(img_sensor, img_ram, LV_ALIGN_OUT_BOTTOM_MID, 0, 35);

    label = lv_label_create(cont);
    lv_obj_add_style(label, &font_style, 0);
    lv_label_set_text(label, "N/A");
    lv_obj_align_to(label, img_sensor, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
    pdat[2] = label;


    //SDCard
    const  BoardsConfigure_t *boards = amoled.getBoarsdConfigure();
    if (boards->sd) {
        lv_obj_t *img_sd = lv_img_create(cont);
        lv_img_set_src(img_sd, &icon_micro_sd);
        lv_obj_align_to(img_sd, img_ram, LV_ALIGN_OUT_RIGHT_MID, 40, 0);

        label = lv_label_create(cont);
        lv_obj_add_style(label, &font_style, 0);
        if (SD.cardType() != CARD_NONE) {
            lv_label_set_text_fmt(label, "%.2fG", SD.cardSize() / 1024 / 1024 / 1024.0);
        } else {
            lv_label_set_text(label, "N/A");
        }
        lv_obj_align_to(label, img_sd, LV_ALIGN_OUT_BOTTOM_MID, 0, 5);
    }

    lv_timer_create([](lv_timer_t *t) {

        lv_obj_t **p = (lv_obj_t **)t->user_data;
        uint16_t vol =   amoled.getBattVoltage();
        lv_label_set_text_fmt(p[0], "%u mV", vol);

        const  BoardsConfigure_t *boards = amoled.getBoarsdConfigure();
        if (boards->pmu) {
            vol =   amoled.getVbusVoltage();
            lv_label_set_text_fmt(p[1], "%u mV", vol);

            if (boards->sensor) {
                float lux = amoled.getLux();
                lv_label_set_text_fmt(p[2], "%.1f lux", lux);
            }
        }

    }, 1000, pdat);

}

static volatile bool smartConfigStart = false;
static lv_timer_t *wifi_timer = NULL;
static uint32_t wifi_timer_counter = 0;
static uint32_t wifi_connnect_timeout = 60;

static void wifi_config_event_handler(lv_event_t *e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *btn = (lv_obj_t *)lv_event_get_target(e);
    lv_obj_t *label = (lv_obj_t *)lv_event_get_user_data(e);
    if (code == LV_EVENT_CLICKED) {
        if (smartConfigStart) {
            lv_label_set_text(label, "Config Start");
            if (wifi_timer) {
                lv_timer_del(wifi_timer);
                wifi_timer = NULL;
            }
            WiFi.stopSmartConfig();
            lv_obj_clear_state(btn, LV_STATE_CHECKED);
            Serial.println("return smart Config has Start;");
            smartConfigStart = false;
            return ;
        }
        WiFi.disconnect();
        smartConfigStart = true;
        WiFi.beginSmartConfig();
        lv_label_set_text(label, "Config Stop");
        lv_obj_add_state(btn, LV_STATE_CHECKED);

        wifi_timer =  lv_timer_create([](lv_timer_t *t) {
            lv_obj_t *btn = (lv_obj_t *)t->user_data;
            lv_obj_t *label = lv_obj_get_child(btn, 0);
            bool destory = false;
            wifi_timer_counter++;
            if (wifi_timer_counter > wifi_connnect_timeout && !WiFi.isConnected()) {
                Serial.println("Connect timeout!");
                destory = true;
                lv_label_set_text(label, "Time Out");
            }
            if (WiFi.isConnected() ) {
                Serial.println("WiFi has connected!");
                Serial.printf("SSID:%s\r\n", WiFi.SSID().c_str());
                Serial.printf("PSW:%s\r\n", WiFi.psk().c_str());
                destory = true;
                String IP = WiFi.localIP().toString();
                lv_label_set_text(label, IP.c_str());
            }
            if (destory) {
                WiFi.stopSmartConfig();
                smartConfigStart = false;
                lv_timer_del(wifi_timer);
                wifi_timer = NULL;
                wifi_timer_counter = 0;
                lv_obj_clear_state(btn, LV_STATE_CHECKED);
            }
            // Every seconds check conected
        }, 1000, btn);
    }
}

void createWiFiConfigUI(lv_obj_t *parent)
{
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_bg_color(cont, lv_color_black(), 0);

    if (lv_disp_get_ver_res(NULL) > 300) {
        lv_obj_set_size(cont, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL));
    } else {
        lv_obj_set_size(cont, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL) * 2 + 20);
    }

    lv_obj_remove_style(cont, 0, LV_PART_SCROLLBAR);
    lv_obj_set_style_border_width(cont, 0, 0);

    String txt = "Use ESP Touch App Configure your network\n";
    txt += "1. Install ESPTouch App\n";
    txt += "2. Turn on ESPTouch -> Click EspTouch\n";
    txt += "3. Enter Your WiFi Password,\n\tOther setting use default\n";
    txt += "4. Confirm\n";
    txt += "5. Click Config WiFi Button\n";
    txt += "6. Wait config done\n";


    lv_obj_t *label, *tips_label ;
    tips_label = lv_label_create(cont);
    lv_obj_set_width(tips_label, LV_PCT(100));
    lv_label_set_long_mode(tips_label, LV_LABEL_LONG_SCROLL);
    lv_obj_set_style_text_color(tips_label, lv_color_white(), LV_PART_MAIN);
    lv_label_set_text(tips_label, txt.c_str());
    lv_obj_align(tips_label, LV_ALIGN_TOP_MID, 0, 20);

    // lv_obj_t *left_arrow = lv_label_create(cont);
    // lv_label_set_text_fmt(left_arrow, ">");
    // lv_obj_set_style_text_color(left_arrow, lv_color_make(255, 255, 255), LV_PART_MAIN);
    // lv_obj_set_style_text_align(left_arrow, LV_TEXT_ALIGN_CENTER, 0);
    // lv_obj_align_to(left_arrow, label, LV_ALIGN_TOP_LEFT, 0, 0);

    const char *android_url = "https://github.com/EspressifApp/EsptouchForAndroid/releases/tag/v2.0.0/esptouch-v2.0.0.apk";
    const char *ios_url = "https://apps.apple.com/cn/app/espressif-esptouch/id1071176700";


    lv_color_t bg_color = lv_palette_lighten(LV_PALETTE_GREEN, 5);
    lv_color_t fg_color = lv_palette_darken(LV_PALETTE_NONE, 4);

    lv_coord_t size = 120;
    lv_obj_t *android_rq_code = lv_qrcode_create(cont, size, fg_color, bg_color);
    lv_qrcode_update(android_rq_code, android_url, strlen(android_url));
    lv_obj_align_to(android_rq_code, tips_label, LV_ALIGN_OUT_BOTTOM_LEFT, 30, 5);
    /*Add a border with bg_color*/
    lv_obj_set_style_border_color(android_rq_code, bg_color, 0);
    lv_obj_set_style_border_width(android_rq_code, 5, 0);
    label = lv_label_create(cont);
    lv_label_set_text(label, "Android" );
    lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align_to(label, android_rq_code, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);

    lv_obj_t *ios_rq_code = lv_qrcode_create(cont, size, fg_color, bg_color);
    lv_qrcode_update(ios_rq_code, ios_url, strlen(ios_url));
    lv_obj_align_to(ios_rq_code, android_rq_code, LV_ALIGN_OUT_RIGHT_MID, 40, -5);

    /*Add a border with bg_color*/
    lv_obj_set_style_border_color(ios_rq_code, bg_color, 0);
    lv_obj_set_style_border_width(ios_rq_code, 5, 0);
    label = lv_label_create(cont);
    lv_label_set_text(label, "IOS" );
    lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align_to(label, ios_rq_code, LV_ALIGN_OUT_BOTTOM_MID, 0, 10);


    //BTN
    lv_obj_t *btn =  lv_btn_create(cont);
    label = lv_label_create(btn);
    lv_label_set_text(label, "Config WiFi");
    lv_obj_set_width(btn, 300);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align_to(label, btn, LV_ALIGN_CENTER, 0, 0);
    lv_obj_add_event_cb(btn, wifi_config_event_handler, LV_EVENT_CLICKED, label);
    lv_obj_center(btn);

    if (lv_disp_get_ver_res(NULL) > 300) {
        lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, -30);
    } else {
        lv_obj_align(btn, LV_ALIGN_BOTTOM_MID, 0, 0);
    }

}

void tileview_change_cb(lv_event_t *e)
{
    lv_obj_t *tileview = lv_event_get_target(e);
    pageId = lv_obj_get_index(lv_tileview_get_tile_act(tileview));
    lv_event_code_t c = lv_event_get_code(e);
    Serial.print("Code : ");
    Serial.print(c);
    uint32_t count =  lv_obj_get_child_cnt(tileview);
    Serial.print(" Count:");
    Serial.print(count);
    Serial.print(" pageId:");
    Serial.println(pageId);

}

void factoryGUI(void)
{
    static lv_style_t bgStyle;
    lv_style_init(&bgStyle);
    lv_style_set_bg_color(&bgStyle, lv_color_black());

    tileview = lv_tileview_create(lv_scr_act());
    lv_obj_add_style(tileview, &bgStyle, LV_PART_MAIN);
    lv_obj_set_size(tileview, lv_disp_get_hor_res(NULL), lv_disp_get_ver_res(NULL));
    lv_obj_add_event_cb(tileview, tileview_change_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_set_scrollbar_mode(tileview, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *t1 = lv_tileview_add_tile(tileview, 0, 0, LV_DIR_HOR | LV_DIR_BOTTOM);
    lv_obj_t *t2 = lv_tileview_add_tile(tileview, 1, 0, LV_DIR_HOR | LV_DIR_BOTTOM);
    lv_obj_t *t3 = lv_tileview_add_tile(tileview, 2, 0, LV_DIR_HOR | LV_DIR_BOTTOM);
    lv_obj_t *t4 = lv_tileview_add_tile(tileview, 3, 0,  LV_DIR_HOR | LV_DIR_BOTTOM);
    // lv_obj_t *t5 = lv_tileview_add_tile(tileview, 4, 0,  LV_DIR_HOR | LV_DIR_BOTTOM);
    // lv_obj_t *t6 = lv_tileview_add_tile(tileview, 5, 0, LV_DIR_HOR | LV_DIR_BOTTOM);
    // lv_obj_t *t7 = lv_tileview_add_tile(tileview, 6, 0, LV_DIR_HOR | LV_DIR_BOTTOM);
    createStartupScreen(lv_scr_act());
    // DNS(t1);
    DNS_UI(t1);  // Use the new DNS UI function
    createBrightnessUI(t2);
    createDeviceInfoUI(t3);
    createWiFiConfigUI(t4);

}


void selectNextItem()
{
    static int id = 0;
    id++;
    id %= 6;
    lv_obj_set_tile_id(tileview, id, 0, LV_ANIM_ON);
}
