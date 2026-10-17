// Compile the production wrapper against the real Bosch headers and faulted bus/Bosch calls.
#include <accel_gyro_bmi270.h>
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <string.h>
#include <time.h>

static int adds, removes, inits, configs, enables, disables;
static int addResult, removeResult, initResult, configResult, enableResult, disableResult, readResult;
static struct bmi2_int_pin_config pins;
static uint16_t irqStatus;
static int8_t irqResult;
static atomic_int boschActive;
static void enter(void)
{
    assert(atomic_fetch_add(&boschActive, 1) == 0);
}
static void leave(void)
{
    assert(atomic_fetch_sub(&boschActive, 1) == 1);
}

esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t bus, const i2c_device_config_t *cfg,
                                    i2c_master_dev_handle_t *out)
{
    ++adds;
    if (addResult) return addResult;
    *out = (void *)3;
    return ESP_OK;
}
esp_err_t i2c_master_bus_rm_device(i2c_master_dev_handle_t dev)
{
    ++removes;
    return removeResult;
}
esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t d, const uint8_t *w, size_t wn, uint8_t *r, size_t rn,
                                      int t)
{
    return ESP_OK;
}
esp_err_t i2c_master_transmit(i2c_master_dev_handle_t d, const uint8_t *w, size_t n, int t)
{
    return ESP_OK;
}
const char *esp_err_to_name(esp_err_t e)
{
    return "injected";
}
int64_t esp_timer_get_time(void)
{
    static int64_t time;
    return ++time;
}
int8_t bmi270_init(struct bmi2_dev *d)
{
    enter();
    ++inits;
    leave();
    return initResult;
}
int8_t bmi2_get_sensor_config(struct bmi2_sens_config *c, uint8_t n, struct bmi2_dev *d)
{
    enter();
    leave();
    return 0;
}
int8_t bmi270_get_sensor_config(struct bmi2_sens_config *c, uint8_t n, struct bmi2_dev *d)
{
    enter();
    leave();
    return 0;
}
int8_t bmi270_set_sensor_config(struct bmi2_sens_config *c, uint8_t n, struct bmi2_dev *d)
{
    enter();
    ++configs;
    if (n == 2) {
        assert(c[0].cfg.acc.range == BMI2_ACC_RANGE_4G && c[0].cfg.acc.odr == BMI2_ACC_ODR_200HZ);
        assert(c[1].cfg.gyr.range == BMI2_GYR_RANGE_1000 && c[1].cfg.gyr.odr == BMI2_GYR_ODR_200HZ);
    }
    leave();
    return configResult;
}
int8_t bmi270_sensor_enable(const uint8_t *s, uint8_t n, struct bmi2_dev *d)
{
    enter();
    ++enables;
    leave();
    return enableResult;
}
int8_t bmi270_sensor_disable(const uint8_t *s, uint8_t n, struct bmi2_dev *d)
{
    enter();
    ++disables;
    leave();
    return disableResult;
}
int8_t bmi2_set_remap_axes(const struct bmi2_remap *r, struct bmi2_dev *d)
{
    enter();
    leave();
    return 0;
}
int8_t bmi270_map_feat_int(const struct bmi2_sens_int_config *c, uint8_t n, struct bmi2_dev *d)
{
    enter();
    leave();
    return 0;
}
int8_t bmi2_get_int_pin_config(struct bmi2_int_pin_config *c, struct bmi2_dev *d)
{
    enter();
    *c = pins;
    leave();
    return 0;
}
int8_t bmi2_set_int_pin_config(const struct bmi2_int_pin_config *c, struct bmi2_dev *d)
{
    enter();
    pins = *c;
    leave();
    return 0;
}
int8_t bmi2_get_int_status(uint16_t *s, struct bmi2_dev *d)
{
    enter();
    *s = irqStatus;
    leave();
    return irqResult;
}
int8_t bmi2_get_sensor_data(struct bmi2_sens_data *s, struct bmi2_dev *d)
{
    enter();
    struct timespec delay = {0, 100000};
    nanosleep(&delay, NULL);
    memset(s, 0, sizeof(*s));
    leave();
    return readResult;
}
static void *poll(void *arg)
{
    struct bmi2_sens_data sample;
    for (int i = 0; i < 50; ++i) assert(accel_gyro_bmi270_get_data(&sample));
    return NULL;
}
static void *recover(void *arg)
{
    for (int i = 0; i < 50; ++i) assert(accel_gyro_bmi270_restore_normal());
    return NULL;
}
int main(void)
{
    const i2c_master_bus_handle_t bus = (void *)1;
    assert(accel_gyro_bmi270_init(NULL) == ESP_ERR_INVALID_ARG);
    addResult = ESP_FAIL;
    assert(accel_gyro_bmi270_init(bus) == ESP_FAIL && inits == 0);
    addResult  = 0;
    initResult = BMI2_E_COM_FAIL;
    assert(accel_gyro_bmi270_init(bus) == ESP_FAIL && removes == 1);
    removeResult = ESP_FAIL;
    assert(accel_gyro_bmi270_init(bus) == ESP_FAIL && removes == 2);
    int previousAdds = adds;
    initResult       = 0;
    assert(accel_gyro_bmi270_init(bus) == ESP_OK && adds == previousAdds);  // Reuse retained handle.
    int previousInits = inits;
    assert(accel_gyro_bmi270_init(bus) == ESP_OK && inits == previousInits);
    assert(accel_gyro_bmi270_init((void *)2) == ESP_ERR_INVALID_STATE);
    irqStatus = BMI270_ANY_MOT_STATUS_MASK;
    assert(accel_gyro_bmi270_check_irq());
    irqStatus = BMI270_WRIST_WAKE_UP_STATUS_MASK;
    assert(accel_gyro_bmi270_check_irq());
    irqStatus = 0;
    assert(!accel_gyro_bmi270_check_irq());
    irqStatus = BMI270_ANY_MOT_STATUS_MASK | BMI270_WRIST_WAKE_UP_STATUS_MASK;
    irqResult = BMI2_E_COM_FAIL;
    assert(!accel_gyro_bmi270_check_irq());  // Never interpret a failed read as a wake IRQ.
    irqResult = 0;
    assert(accel_gyro_bmi270_enable_sensor());
    configResult = BMI2_E_COM_FAIL;
    assert(!accel_gyro_bmi270_enable_sensor());
    configResult = 0;
    enableResult = BMI2_E_COM_FAIL;
    assert(!accel_gyro_bmi270_enable_sensor());
    enableResult        = 0;
    disableResult       = BMI2_E_COM_FAIL;
    int previousConfigs = configs;
    assert(!accel_gyro_bmi270_restore_normal());
    assert(configs == previousConfigs + 1);  // Restore sampling despite feature-disable failure.
    disableResult = 0;
    assert(accel_gyro_bmi270_motion_irq());
    assert(accel_gyro_bmi270_restore_normal());
    assert(pins.pin_cfg[0].output_en == BMI2_INT_OUTPUT_DISABLE);
    assert(pins.pin_cfg[1].output_en == BMI2_INT_OUTPUT_DISABLE);
    struct bmi2_sens_data sample;
    readResult = BMI2_E_COM_FAIL;
    assert(!accel_gyro_bmi270_get_data(&sample));
    readResult = 0;
    pthread_t reader, recovery;
    assert(pthread_create(&reader, NULL, poll, NULL) == 0);
    assert(pthread_create(&recovery, NULL, recover, NULL) == 0);
    pthread_join(reader, NULL);
    pthread_join(recovery, NULL);
    assert(atomic_load(&boschActive) == 0);
}
