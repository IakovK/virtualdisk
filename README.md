# virtualdisk
virtual disk for Windows sample
It is a sample that illustrates how to create virtual disks - similar to Microsoft VHD services.

The sample consists of the following components:
driver - in subfolder bus;
service - user mode companion of driver in subfolder umode\disksrv;
control program - to manage virtual disks, in subfolder umode\diskctl;
sample plugin to handle VMWare vmdk files - in subfolder umode\vmdkplug.

Driver exposes disks to the operating system.
After it receives a command from the service, it announces that the disk is ready to use.
The disk here is virtual analog of hard disk - not a partition.
Once the disk is announced, the OS will send various requests to the driver; read and write in particular.
Read and write requests are handled by service; when the driver receives the request, it signals the service
and the service executes the request.
Once the request is completed, the service signals driver back and the driver signals OS that the request is completed.

All the storage is managed by the service - in user mode. So, all the access rights can be easily enforced if needed.
The service itself implements simple block storage in files. But it allows plugins. Plugin can implement any storage you want.
Sample plugin is included that illustrates how to handle VMWare vmdk files.

To build:
use Visual Studio and Windows Driver Kit.To build the sample plugin VMWare virtual disk sdk is needed.
