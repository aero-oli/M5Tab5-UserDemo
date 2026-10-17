#include <modules/rtc/rx8130_calendar.h>
#include <array>
#include <cassert>
#include <cstring>
#include <vector>

struct FakeRtc : modules::I2cDevice {
    std::array<uint8_t, 64> regs{};
    struct Op { bool write; uint8_t reg; size_t count; };
    std::vector<Op> ops;
    int failAt = -1;
    int corruptAt = -1;
    bool present = true;
    bool tickAfterWrite = false;
    bool probe() override { return present; }
    uint8_t address() const override { return 0x32; }
    bool readRegister(uint8_t reg, uint8_t* out, size_t count) override {
        ops.push_back({false,reg,count});
        if (!present || static_cast<int>(ops.size()) - 1 == failAt) return false;
        std::memcpy(out, regs.data()+reg, count);
        if (static_cast<int>(ops.size()) - 1 == corruptAt) out[0] ^= 1;
        return true;
    }
    bool writeRegister(uint8_t reg, const uint8_t* in, size_t count) override {
        ops.push_back({true,reg,count});
        if (!present || static_cast<int>(ops.size()) - 1 == failAt) return false;
        if (reg == 0x1d) regs[reg] &= *in; // Hardware flags are write-zero-to-clear.
        else std::memcpy(regs.data()+reg, in, count);
        if (tickAfterWrite && reg==0x10) {
            // 2024-02-29 23:59:59 advances across a month/day boundary.
            const uint8_t next[] = {0,0,0,0x20,1,3,0x24};
            std::memcpy(regs.data()+0x10,next,sizeof(next));
        }
        return true;
    }
    FakeRtc() { // Saturday, 17 October 2026, 09:08:07.
        const uint8_t initial[] = {0x07,0x08,0x09,0x40,0x17,0x10,0x26};
        std::memcpy(regs.data()+0x10, initial, sizeof(initial));
        regs[0x1c]=0x12; regs[0x1e]=0x10; regs[0x1f]=0x30;
        regs[0x1a]=0x55; regs[0x1b]=0x01;
    }
};
static tm chosen()
{
    tm result{}; result.tm_year=124; result.tm_mon=1; result.tm_mday=29;
    result.tm_hour=23; result.tm_min=59; result.tm_sec=58;
    return result;
}
int main()
{
    FakeRtc fake; rtc::Rx8130Calendar calendar(fake);
    tm value{}; bool needs=false;
    assert(calendar.read(value, needs));
    assert(value.tm_year==126 && value.tm_mon==9 && value.tm_mday==17 && value.tm_wday==6);
    for (const auto& op : fake.ops) assert(!op.write);
    const auto untouched = fake.regs;
    fake.ops.clear();
    assert(calendar.set(chosen(), false, value));
    assert(value.tm_mon==1 && value.tm_wday==4 && fake.regs[0x13]==0x10);
    assert(fake.regs[0x15]==0x02 && fake.regs[0x16]==0x24);
    for (const auto& op : fake.ops) if (op.write) assert(op.reg==0x10 && op.count==7);
    for (size_t reg=0x17;reg<fake.regs.size();++reg) assert(fake.regs[reg]==untouched[reg]);
    const auto normalOps=fake.ops;
    for (size_t fail=0;fail<normalOps.size();++fail) {
        FakeRtc fault; fault.failAt=static_cast<int>(fail);
        rtc::Rx8130Calendar driver(fault);
        assert(!driver.set(chosen(),false,value));
        assert(!driver.error().empty());
    }
    FakeRtc ticking; ticking.tickAfterWrite=true;
    rtc::Rx8130Calendar tickingDriver(ticking);
    tm midnight=chosen(); midnight.tm_sec=59;
    assert(tickingDriver.set(midnight,false,value));
    assert(value.tm_mon==2 && value.tm_mday==1 && value.tm_hour==0);
    for (int day=0;day<7;++day) {
        FakeRtc weekday; weekday.regs[0x14]=static_cast<uint8_t>(0x17+day);
        if (day>=3) weekday.regs[0x14]=static_cast<uint8_t>(0x20+day-3);
        weekday.regs[0x13]=static_cast<uint8_t>(1u<<((day+6)%7));
        rtc::Rx8130Calendar weekdayDriver(weekday);
        assert(weekdayDriver.read(value,needs) && value.tm_wday==(day+6)%7);
    }
    // Malformed BCD, impossible date, multi-bit weekday and wrong weekday fail closed.
    for (auto bad : std::vector<std::pair<int,uint8_t>>{{0x10,0x6a},{0x14,0x32},{0x13,3},{0x13,1},{0x15,0}}) {
        FakeRtc fault; fault.regs[bad.first]=bad.second; rtc::Rx8130Calendar driver(fault);
        tm unchanged{}; unchanged.tm_year=123;
        assert(!driver.read(unchanged,needs) && unchanged.tm_year==123);
    }
    for (int reg : {0x1d,0x1e}) {
        FakeRtc fault; fault.regs[reg] |= reg==0x1d ? 2 : 0x40;
        rtc::Rx8130Calendar driver(fault); const auto before=fault.regs;
        assert(!driver.read(value,needs) && needs);
        assert(!driver.set(chosen(),false,value));
        assert(fault.regs==before); // No implicit initialization.
    }
    FakeRtc recovery; recovery.regs[0x1d]=0x3e;
    rtc::Rx8130Calendar recoveryDriver(recovery);
    const auto beforeRecovery=recovery.regs;
    assert(recoveryDriver.clearWakeIrq());
    assert(recovery.regs==beforeRecovery); // Startup does not hide VLF or touch invalid configuration.
    recovery.ops.clear();
    assert(recoveryDriver.set(chosen(),true,value));
    assert(recovery.regs[0x1d]==0 && recovery.regs[0x1e]==0);
    assert(recovery.regs[0x1c]==4 && recovery.regs[0x1a]==0 && recovery.regs[0x1b]==0);
    assert(recovery.regs[0x17]==0x80 && recovery.regs[0x1f]==0x30);
    const auto recoveryOps=recovery.ops;
    for (size_t fail=0;fail<recoveryOps.size();++fail) {
        FakeRtc fault; fault.regs[0x1d]=2; fault.failAt=static_cast<int>(fail);
        rtc::Rx8130Calendar driver(fault);
        assert(!driver.set(chosen(),true,value));
        assert(!driver.error().empty());
    }
    // Readback mismatches abort initialization before VLF acknowledgment.
    for (size_t i=0;i<recoveryOps.size();++i) if (!recoveryOps[i].write && i>2 && recoveryOps[i-1].write) {
        FakeRtc fault; fault.regs[0x1d]=2; fault.corruptAt=static_cast<int>(i);
        rtc::Rx8130Calendar driver(fault);
        assert(!driver.set(chosen(),true,value));
    }
    FakeRtc flags; flags.regs[0x1d]=0xbd; rtc::Rx8130Calendar flagsDriver(flags);
    assert(flagsDriver.clearWakeIrq());
    assert(flags.regs[0x1d]==0x85); // Preserve reset/backup, clear only IRQ flags.
    tm invalid=chosen(); invalid.tm_year=125;
    fake.ops.clear(); assert(!calendar.set(invalid,false,value) && fake.ops.empty());
    invalid=chosen(); invalid.tm_mon=12;
    assert(!calendar.set(invalid,false,value));
    invalid=chosen(); invalid.tm_year=200;
    assert(!calendar.set(invalid,false,value));
}
