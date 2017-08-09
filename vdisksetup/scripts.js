function StartService()
{
var objWMIService = GetObject("winmgmts:");
var colServiceList = objWMIService.ExecQuery ("Select * from Win32_Service where Name='disksrv'");

for(e = new Enumerator(colServiceList) ; !e.atEnd() ; e.moveNext())
{
    objService = e.item();
    errReturn = objService.StartService();
	if (errReturn != 0)
	   Session.DoAction ("ScheduleReboot");
}
}
