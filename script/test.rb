require "open3"

riscv_test_path = "/home/blamel/workspace/riscv-tests/isa/"
space_path = "/home/blamel/workspace/space/"

test_binary_file_list = Dir.children(riscv_test_path).select do |f|
  f.match(/.*-p-.*\.bin$/)
end

Dir.chdir(space_path)
test_binary_file_list.each do |name|
  cmd = "./space %s"%[riscv_test_path + name]
  puts cmd
  Open3.popen3(cmd) do |stdin, stdout, stderr, wait_thr|
    while line = stdout.gets
      puts line
    end
  end
end
