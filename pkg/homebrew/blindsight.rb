class Blindsight < Formula
  desc "High-density, terminal-based binary viewer for visual pattern matching"
  homepage "https://github.com/amtal/blindsight#readme"
  url "https://github.com/amtal/blindsight/archive/v0.1.1.tar.gz"
  sha256 "0a82748207dd4157b79d2e373f9463af3fd5abc75472a6cd0a5c757eeaf526ad"

  head "https://github.com/amtal/blindsight.git"

  # see https://discourse.brew.sh/t/1008 for why autoconf is a direct dep.
  # "I had trouble finding the exact semantics of :build" loool brew #1
  depends_on "autoconf" => :build
  depends_on "automake" => :build
  depends_on "libtool" => :build

  # ncurses is built with --enable-widec (which we need for >256 color pairs)
  # header in ncursesw/ncurses.h, library in libncurses.dylib
  depends_on "ncurses"
  depends_on "tcc"
  
  def install
    system "./bootstrap && ./configure && make"
    system "make", "DESTDIR=#{prefix}", "install"
  end

  test do
    system "time gtimeout 1 blindsight `which blindsight` || true"
  end
end
