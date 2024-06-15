#include <stdio.h>
#include <zlib.h>
#include <string.h>
#include <malloc.h>

//最常用的解压缩
void test_compress()
{
	//待压缩的数据
	const char* str = "hello word，I’m wanshi teacher,thank's student!\n"
		"hello word，I’m wanshi teacher,thank's student!\n"
		"hello word，I’m wanshi teacher,thank's student!\n"
		"hello word，I’m wanshi teacher,thank's student!\n";
	uLongf str_len = strlen(str);

	//目标缓冲区
	//char dstbuf[BUFSIZ];
	//uLongf dst_len = BUFSIZ;

	//compressBound 根据传入的原始数据大小，猜测压缩之后的大小，可以用这个大小来分配内存
	uLongf dst_len = compressBound(str_len);
	char* dstbuf = malloc(dst_len * sizeof(char));

	printf("%d str:%s\n", str_len, str);

	//压缩数据
	{
		if (Z_OK != compress2(dstbuf, &dst_len, str, str_len, 5)) {
			printf("compress2 failed");
			return;
		}
		//压缩前数据大小5B
		//压缩后数据大小13B
		//为什么越压缩数据越大了？
		printf("%d dstbuf:%.*s\n", dst_len, dst_len, dstbuf);
	}

	//解压数据，无法知道解压之后需要多大的内存，你必须自己保存原始数据大小
	{
		char debuf[BUFSIZ];
		uLongf de_len = BUFSIZ;

		if (Z_OK != uncompress2(debuf, &de_len, dstbuf, &dst_len)) {
			printf("uncompress2 failed!");
			return;
		}
		printf("%lu str:%.*s\n", de_len,de_len, debuf);
	}

	free(dstbuf);
}

//gzip(.gz)文件操作
void test_zip()
{
	//一定要用wb的方式打开，因为压缩文件是二进制文件
	gzFile gz = gzopen("test.gz", "wb");
	if (!gz) {
		printf("gzopen failed\n");
		return;
	}
	//写输入数据
	int len = gzwrite(gz, "hello wold", 10);
	gzfwrite("你好世界", 12, 1, gz);
	gzputs(gz,"123456789");
	for (size_t i = 0; i < 26; i++) {
		gzputc(gz, 'A' + i);
	}
	gzclose(gz);
}

void test_unzip()
{
	gzFile gz = gzopen("test.gz", "rb");
	if (!gz) {
		printf("gzopen failed\n");
		return;
	}
	char buf[BUFSIZ];
	int len =  gzread(gz, buf, BUFSIZ);
	//printf("len:%d buf:%.*s", len, len, buf);
	for (size_t i = 0; i < len; i++) {
		putchar(buf[i]);
	}
	gzclose(gz);
}

//在内存用压缩gzip文件
int gzcompress(Bytef* out_data,uLong* in_out_len,Bytef* data,uLong len)
{
	if (out_data == NULL || in_out_len == NULL || data == NULL || len == 0)
		return -1;

	//准备压缩
	z_stream stream = {0};
	int err = 0;

	//只有windowBits 设置为MAX_WBITS+16,压缩数据中才会待header 和 trailer
	if (Z_OK != deflateInit2(&stream, Z_DEFAULT_COMPRESSION, Z_DEFLATED,
		MAX_WBITS + 16, 8, Z_DEFAULT_STRATEGY)) {
		printf("deflateInit2 failed\n");
		return -1;
	}
	//关联输入输出缓冲区
	stream.next_in = data;				//输入缓冲区
	stream.avail_in = len;				//输入缓冲区长度
	stream.next_out = out_data;			//输出缓冲区
	stream.avail_out = *in_out_len;		//输出缓冲区长度

	//开始压缩
	while (stream.avail_in != 0 && 
		stream.total_out < *in_out_len	//防止输出缓冲区溢出
		) {
		if (Z_OK != deflate(&stream, Z_NO_FLUSH)) {
			printf("deflate failed\n");
			return -1;
		}
	}
	//判断是不是没有写完
	if (stream.avail_in != 0)
		return stream.avail_in;

	//结束压缩
	if (deflate(&stream, Z_FINISH) == Z_STREAM_END) {
		printf("defate finished!\n");
	}

	if (Z_OK != deflateEnd(&stream)) {
		printf("deflateEnd failed\n");
		return -1;
	}
	*in_out_len = stream.total_out;	//压缩之后的总字节数
	return 0;
}

//在内存用解压gzip文件
int gzuncompress(Bytef* out_data, uLong* in_out_len, Bytef* data, uLong len)
{
	if (out_data == NULL || in_out_len == NULL || data == NULL || len == 0)
		return -1;

	//准备压缩
	z_stream stream = {0};
	int err = 0;
	// 有些服务器(特别是带有mod_deflate的Apache)不生成zlib头文件。插入一个模拟头，然后再试一次
    static char dummy_head[2] = {
        0x8 + 0x7 * 0x10,
        (((0x8 + 0x7 * 0x10) * 0x100 + 30) / 31 * 31) & 0xFF,
    };

	stream.next_in = data;
	stream.next_out = out_data;

	//初始化inflate算法
	if (Z_OK != inflateInit2(&stream, MAX_WBITS + 16)) {
		printf("inflateInit2 failed\n");
		return -1;
	}
	//如果输出缓冲区内存还有，并且输入数据没有解压完毕，就一直解压
	while (stream.total_out < *in_out_len && stream.total_in < len)
	{
		stream.avail_in = stream.avail_out = 1;	//强制小缓冲
		err = inflate(&stream, Z_NO_FLUSH);
		if (Z_STREAM_END == err) {
			break;
		}
		else  if (Z_OK != err) {
			printf("infate error\n");
			//如果是数据错误，那么尝试插入模拟头重试一次
			if (Z_DATA_ERROR == err) {
				stream.next_in = dummy_head;
				stream.avail_in = sizeof(dummy_head);
				if (Z_OK != inflate(&stream, Z_NO_FLUSH)) {
					return -1;
				}
			}
		}
	}
	
	if (Z_OK != inflateEnd(&stream)) {
		printf("infalteEnd faild\n");
		return -1;
	}
	*in_out_len = stream.total_out;
	return 0;
}
void test()
{
	//待压缩的数据
	char data[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
	uLongf len = strlen(data);

	//目标缓冲区
	char out_data[BUFSIZ];
	//传入时作为输出缓冲区最大大小，传出时是压缩后的数据大小
	uLongf in_out_len = BUFSIZ;

	if (0 != gzcompress(out_data, &in_out_len, data, len)) {
		printf("gzcompress failed\n");
	}

	char buf[BUFSIZ];
	uLongf buf_len = BUFSIZ;

	if (0 != gzuncompress(buf, &buf_len, out_data, in_out_len)) {
		printf("gzuncompress failed\n");
	}
	printf("len:%lu buf:%.*s\n", buf_len,buf_len, buf);
}

int main(int argc, char* argv[])
{
	//test_compress();
	//test_zip();
	//test_unzip();
	test();
	return 0;
}